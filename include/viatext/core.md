MODULE ViaTextCore

  ## DOC
  ## @description: Minimal ViaText node core — accepts Package, exposes Message, emits Package.
  ##               Keeps node identity, runs a tick->process loop, and handles a few commands.
  ##               Transport, routing, and full reassembly policy are deferred to later versions.
  ## @notes: No ArgParser. Keys in Package.args are preserved exactly as provided ("-m", "--ttl", etc.).
  IMPORT Package
  IMPORT Message
  IMPORT MessageID

  # ---------- Types ----------

  TYPE CoreState {
    node_id: String,             # callsign owned by this node
    tick_count: Number,
    uptime_ms: Number,
    last_ms: Number,

    inbox: List<Package>,        # inbound from wrappers
    outbox: List<Package>,       # outbound to wrappers

    recent_sequences: List<Number>,   # small ring for dedupe of complete messages
    recent_sequences_cap: Number,     # e.g., 64

    # Fragment scaffolding (defined, not fully used yet)
    frag_cap: Number,                 # policy cap for total parts (e.g., 8)
    inflight_cap: Number,             # max sequences tracked (e.g., 4)

    # Placeholder for future fragment tracking (not active in MVP)
    inflight_fragments: List<Object>  # reserved; structure defined below but not enforced
  } END TYPE

  TYPE InflightFragment {
    sequence: Number,                 # MessageID.sequence
    total: Number,                    # expected parts
    received_count: Number,
    received_bitmap: Number,          # bitmask if total <= 8; else future use
    parts: List<String>,              # holds raw fragment payload slices (order by part)
    first_seen_ms: Number,
    basis_args: Object                # Package.args from first seen fragment (future use)
  } END TYPE

  # ---------- Lifecycle API (wrappers call these) ----------

  ## DOC
  ## @description: Initialize core with node_id and small policy caps.
  DEFINE init(node_id: String): CoreState
    SET state: CoreState = CoreState()
    state.node_id = node_id
    state.tick_count = 0
    state.uptime_ms = 0
    state.last_ms = 0

    state.inbox = List()
    state.outbox = List()

    state.recent_sequences = List()
    state.recent_sequences_cap = 64

    state.frag_cap = 8
    state.inflight_cap = 4
    state.inflight_fragments = List()

    RETURN state
  END DEFINE

  ## DOC
  ## @description: Enqueue an inbound Package from any wrapper (LoRa, Linux, Serial).
  ## @return: Boolean — true if enqueued, false if queue is full (future policy).
  DEFINE add_message(state: CoreState, pkg: Package): Boolean
    CALL ListAppend(state.inbox, pkg)
    RETURN true
  END DEFINE

  ## DOC
  ## @description: Advance time; process at most one inbound item.
  DEFINE tick(state: CoreState, now_ms: Number): Void
    IF state.last_ms == 0
      state.last_ms = now_ms
    END IF
    SET delta: Number = now_ms - state.last_ms
    state.uptime_ms = state.uptime_ms + delta
    state.last_ms = now_ms
    state.tick_count = state.tick_count + 1

    CALL process_one(state, now_ms)
  END DEFINE

  ## DOC
  ## @description: Dequeue next outbound Package for the wrapper to transmit/handle.
  ## @return: Package | NULL
  DEFINE get_message(state: CoreState): Package | NULL
    IF CALL ListIsEmpty(state.outbox)
      RETURN NULL
    END IF
    SET out_pkg: Package = CALL ListPopFront(state.outbox)
    RETURN out_pkg
  END DEFINE

  ## DOC
  ## @description: Update node identity.
  DEFINE set_node_id(state: CoreState, node_id: String): Void
    state.node_id = node_id
  END DEFINE

  ## DOC
  ## @description: Read node identity.
  ## @return: String
  DEFINE get_node_id(state: CoreState): String
    RETURN state.node_id
  END DEFINE

  # ---------- Internal processing ----------

  ## DOC
  ## @description: Pop one Package, try to form a Message, and dispatch if complete.
  DEFINE process_one(state: CoreState, now_ms: Number): Void
    IF CALL ListIsEmpty(state.inbox)
      RETURN
    END IF

    SET in_pkg: Package = CALL ListPopFront(state.inbox)

    # Build a Message from this Package (stamp "<hex10>~from~to~data" inside payload).
    SET msg: Message = Message(in_pkg)

    IF NOT CALL msg.is_valid()
      # For MVP: drop invalid messages silently (future: log to outbox as diagnostic).
      RETURN
    END IF

    # Basic dedupe for complete messages (by sequence). Future: include sender key if needed.
    IF CALL contains_sequence(state.recent_sequences, msg.sequence())
      RETURN
    END IF

    # Fragment handling policy (MVP):
    # Keep fragments "as-is" for future reassembly; do not dispatch until complete.
    IF msg.total() > 1
      CALL store_fragment_stub(state, msg, now_ms)   # placeholder, minimal bookkeeping only
      RETURN
    END IF

    # At this point, message is logically complete (1 of 1). Dispatch by args.
    CALL ListAppend(state.recent_sequences, msg.sequence())
    CALL trim_recent_sequences(state)

    CALL dispatch(state, msg)
  END DEFINE

  ## DOC
  ## @description: Route to minimal built-in handlers based on Package.args flags.
  DEFINE dispatch(state: CoreState, msg: Message): Void
    # Order is explicit; only one branch runs in MVP.
    IF CALL msg.flag("-m")
      CALL handle_message(state, msg)
      RETURN
    END IF

    IF CALL msg.flag("-p")
      CALL handle_ping(state, msg)
      RETURN
    END IF

    IF CALL msg.flag("-ack")
      CALL handle_ack(state, msg)
      RETURN
    END IF

    IF CALL msg.flag("--set-id")
      CALL handle_set_id(state, msg)
      RETURN
    END IF

    # Unknown directive: ignore in MVP (future: emit "-unknown" event)
  END DEFINE

  # ---------- Minimal handlers (hard-coded for MVP) ----------

  ## DOC
  ## @description: Standard message delivery. If addressed to this node, emit delivered + optional ACK.
  DEFINE handle_message(state: CoreState, msg: Message): Void
    IF CALL msg.to() == state.node_id
      IF CALL msg.requests_ack()
        SET ack_pkg: Package = CALL make_ack_package(state, msg)
        CALL ListAppend(state.outbox, ack_pkg)
      END IF

      SET delivered_pkg: Package = CALL make_delivered_package(state, msg)
      CALL ListAppend(state.outbox, delivered_pkg)
    ELSE
      # Not addressed to this node — forwarding is wrapper policy; core stays silent.
    END IF
  END DEFINE

  ## DOC
  ## @description: Respond to ping with pong.
  DEFINE handle_ping(state: CoreState, msg: Message): Void
    SET pong_pkg: Package = CALL make_pong_package(state, msg)
    CALL ListAppend(state.outbox, pong_pkg)
  END DEFINE

  ## DOC
  ## @description: Acknowledgment received — pass through as an event in MVP.
  DEFINE handle_ack(state: CoreState, msg: Message): Void
    # Future: correlate with pending deliveries. MVP: surface event for wrappers/UI.
    SET evt: Package = CALL make_ack_event_package(state, msg)
    CALL ListAppend(state.outbox, evt)
  END DEFINE

  ## DOC
  ## @description: Set node ID (trusted contexts only).
  DEFINE handle_set_id(state: CoreState, msg: Message): Void
    # Read target id from message text or args; MVP: use msg.text() as new id.
    SET new_id: String = CALL msg.text()
    IF new_id != ""
      state.node_id = new_id
      SET conf: Package = CALL make_id_set_event(state, new_id)
      CALL ListAppend(state.outbox, conf)
    END IF
  END DEFINE

  # ---------- Helpers: recent-seq ring & fragment stubs ----------

  ## DOC
  ## @description: Return true if seq is found in recent_sequences.
  ## @return: Boolean
  DEFINE contains_sequence(buf: List<Number>, seq: Number): Boolean
    FOR EACH x IN buf
      IF x == seq
        RETURN true
      END IF
    END FOR
    RETURN false
  END DEFINE

  ## DOC
  ## @description: Keep recent_sequences within cap (drop oldest first).
  DEFINE trim_recent_sequences(state: CoreState): Void
    WHILE CALL ListSize(state.recent_sequences) > state.recent_sequences_cap
      CALL ListPopFront(state.recent_sequences)
    END WHILE
  END DEFINE

  ## DOC
  ## @description: Placeholder: store fragment metadata for future reassembly (no dispatch).
  DEFINE store_fragment_stub(state: CoreState, msg: Message, now_ms: Number): Void
    # MVP does not assemble; simply note that a fragment was seen.
    # Future: upsert entry keyed by msg.sequence(), set bit for msg.part(), store msg.pkg.payload slice.
    # No output emitted here.
    RETURN
  END DEFINE

  # ---------- Package builders (sensible outputs) ----------

  ## DOC
  ## @description: Build an ACK reply for a delivered message.
  ## @return: Package
  DEFINE make_ack_package(state: CoreState, msg: Message): Package
    # Compose a new Message that mirrors the sequence and sets ACK flag.
    SET id: MessageID = MessageID(msg.sequence(), 0, 1, msg.hops(), /*flags set via msg setters later*/ 0)
    CALL id.set_is_acknowledgment(true)
    CALL id.set_request_ack(false)

    # Swap from/to and set body "ACK"
    SET out_msg: Message = Message(id, state.node_id, CALL msg.from(), "ACK")

    # Build outbound Package with directive flag "-ack"
    SET out_pkg: Package = Package()
    SET stamp: String = CALL out_msg.to_payload_stamp_copy()
    out_pkg.payload = stamp
    CALL out_pkg.args.set_flag("-ack")
    CALL out_pkg.args.set("--to", CALL msg.from())
    CALL out_pkg.args.set("--from", state.node_id)
    RETURN out_pkg
  END DEFINE

  ## DOC
  ## @description: Build a delivered event for local consumption.
  ## @return: Package
  DEFINE make_delivered_package(state: CoreState, msg: Message): Package
    SET out_pkg: Package = Package()
    # Echo the original payload for simplicity.
    out_pkg.payload = CALL msg.to_payload_stamp_copy()
    CALL out_pkg.args.set_flag("-r")                 # received
    CALL out_pkg.args.set("--to", state.node_id)
    CALL out_pkg.args.set("--from", CALL msg.from())
    RETURN out_pkg
  END DEFINE

  ## DOC
  ## @description: Build a PONG reply to a PING.
  ## @return: Package
  DEFINE make_pong_package(state: CoreState, msg: Message): Package
    # Reuse sequence (policy choice for MVP).
    SET id: MessageID = MessageID(msg.sequence(), 0, 1, msg.hops(), msg.flags())
    SET out_msg: Message = Message(id, state.node_id, CALL msg.from(), "PONG")

    SET out_pkg: Package = Package()
    out_pkg.payload = CALL out_msg.to_payload_stamp_copy()
    CALL out_pkg.args.set_flag("-pong")
    CALL out_pkg.args.set("--to", CALL msg.from())
    CALL out_pkg.args.set("--from", state.node_id)
    RETURN out_pkg
  END DEFINE

  ## DOC
  ## @description: Surface an ACK event as a simple package (UI/log).
  ## @return: Package
  DEFINE make_ack_event_package(state: CoreState, msg: Message): Package
    SET evt: Package = Package()
    # Minimal stamp echo; could be "EVENT~ACK~from~to" later.
    evt.payload = CALL msg.to_payload_stamp_copy()
    CALL evt.args.set_flag("-ack_event")
    CALL evt.args.set("--seq", CALL to_string_number(msg.sequence()))
    RETURN evt
  END DEFINE

  ## DOC
  ## @description: Confirm node id change.
  ## @return: Package
  DEFINE make_id_set_event(state: CoreState, new_id: String): Package
    SET evt: Package = Package()
    evt.payload = "ID_SET~" + new_id
    CALL evt.args.set_flag("-id_set")
    CALL evt.args.set("--node", new_id)
    RETURN evt
  END DEFINE

  # ---------- Utilities ----------

  ## DOC
  ## @description: Convert Number to String (placeholder for platform utility).
  ## @return: String
  DEFINE to_string_number(n: Number): String
    # Host will provide; included for clarity in pseudocode.
    RETURN "0" + ""  # placeholder
  END DEFINE

END MODULE
