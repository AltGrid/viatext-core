# core.BLUEPRINT.md

MODULE ViaTextCore

  ## DOC
  ## @description: Main protocol event loop and message state for ViaText mesh core. All logic is statically defined, heap-free, and AI/MCU safe.
  ## @integration: Built atop Message, ArgParser, TextFragments, uses only blueprint-defined types.
  ## @author: Leo, ChatGPT

  # --- State Type (uses blueprint canonical types)

  TYPE CoreState {
    node_id: String,
    tick_count: Number,
    uptime: Number,
    last_timestamp: Number,
    inbox: List<String>,                # raw incoming arg strings
    outbox: List<String>,               # raw outgoing arg strings
    received_message_ids: List<Number>  # MessageID.sequence, prevents dupes
  } END TYPE

  ## DOC
  ## @description: Initialize state with optional node_id (default empty).
  DEFINE init(optional_node_id: String): CoreState
    SET state: CoreState = CoreState()
    state.node_id = optional_node_id
    state.tick_count = 0
    state.uptime = 0
    state.last_timestamp = 0
    state.inbox = List()
    state.outbox = List()
    state.received_message_ids = List()
    RETURN state
  END DEFINE

  ## DOC
  ## @description: Queue inbound message (raw arg string).
  DEFINE add_message(state: CoreState, arg_string: String): Void
    CALL ListAppend(state.inbox, arg_string)
  END DEFINE

  ## DOC
  ## @description: Main tick: update time, call process().
  DEFINE tick(state: CoreState, current_timestamp: Number): Void
    IF state.last_timestamp == 0
      state.last_timestamp = current_timestamp
    END IF
    state.uptime = state.uptime + (current_timestamp - state.last_timestamp)
    state.last_timestamp = current_timestamp
    state.tick_count = state.tick_count + 1
    CALL process(state)
  END DEFINE

  ## DOC
  ## @description: Process one message from inbox if present.
  DEFINE process(state: CoreState): Void
    IF CALL ListIsEmpty(state.inbox)
      RETURN
    END IF

    # Step 1: Get next message (raw string)
    SET arg_string: String = CALL ListPopFront(state.inbox)
    # Step 2: Parse into canonical ArgParser
    SET parser: ArgParser = ArgParser.from_string(arg_string)
    SET msg_type: String = parser.directive    # Blueprint: parser.directive holds first token

    MATCH msg_type
      CASE "-m"
        CALL handle_message(state, parser)
      CASE "-p"
        CALL handle_ping(state, parser)
      CASE "-mr"
        CALL handle_mesh_report(state, parser)
      CASE "-ack"
        CALL handle_acknowledge(state, parser)
      CASE "-save"
        CALL handle_save(state, parser)
      CASE "-load"
        CALL handle_load(state, parser)
      CASE "-get_time"
        CALL handle_get_time(state, parser)
      CASE "-set_time"
        CALL handle_set_time(state, parser)
      CASE "-id"
        CALL set_node_id(state, parser.get_argument("-id"))
      DEFAULT
        # Unknown or unhandled directive; drop or log
    END MATCH
  END DEFINE

  ## DOC
  ## @description: Handler for standard -m message; dedup, ack, and route.
  DEFINE handle_message(state: CoreState, parser: ArgParser): Void
    SET wire_str: String = parser.get_argument("-data")
    SET message: Message = Message.from_wire_string(wire_str)
    IF CALL ListContains(state.received_message_ids, message.id.sequence)
      RETURN
    END IF
    CALL ListAppend(state.received_message_ids, message.id.sequence)
    IF message.to == state.node_id
      IF message.id.requests_acknowledgment()
        SET ack_args: String = CALL build_ack_args(state, message)
        CALL ListAppend(state.outbox, ack_args)
      END IF
      SET delivered_args: String = CALL build_received_args(state, message)
      CALL ListAppend(state.outbox, delivered_args)
    END IF
    # Not for us: forwarding/mesh logic handled by wrapper
  END DEFINE

  ## DOC
  ## @description: Handler for ping (-p); respond with -pong.
  DEFINE handle_ping(state: CoreState, parser: ArgParser): Void
    SET reply_args: String = StringConcat("-pong -from ", state.node_id)
    CALL ListAppend(state.outbox, reply_args)
  END DEFINE

  ## DOC
  ## @description: Handler for mesh report (-mr).
  DEFINE handle_mesh_report(state: CoreState, parser: ArgParser): Void
    SET mesh_args: String = StringConcat("-mr -from ", state.node_id, " -neigh_count ", get_neighbor_count(state))
    CALL ListAppend(state.outbox, mesh_args)
  END DEFINE

  ## DOC
  ## @description: Handler for ack (-ack); update state or trigger app event.
  DEFINE handle_acknowledge(state: CoreState, parser: ArgParser): Void
    SET ack_id: String = parser.get_argument("-msg_id")
    # App/wrapper decides display, removal, or persistence of ACKs
  END DEFINE

  ## DOC
  ## @description: Utility: set node ID.
  DEFINE set_node_id(state: CoreState, new_id: String): Void
    state.node_id = new_id
  END DEFINE

  ## DOC
  ## @description: Retrieve next outbound message if any.
  DEFINE get_message(state: CoreState): String
    IF CALL ListIsEmpty(state.outbox)
      RETURN ""
    END IF
    RETURN CALL ListPopFront(state.outbox)
  END DEFINE

  ## DOC
  ## @description: Build raw arg string for ACK reply.
  DEFINE build_ack_args(state: CoreState, message: Message): String
    RETURN StringConcat("-ack -from ", state.node_id, " -to ", message.from, " -msg_id ", message.id.sequence)
  END DEFINE

  ## DOC
  ## @description: Build raw arg string for "received" reply.
  DEFINE build_received_args(state: CoreState, message: Message): String
    RETURN StringConcat("-r -from ", message.from, " -data ", message.to_wire_string())
  END DEFINE

  ## DOC
  ## @description: Get neighbor count (stub).
  DEFINE get_neighbor_count(state: CoreState): Number
    RETURN 0
  END DEFINE

  # (Handlers for save/load/get_time/set_time can be customized as needed.)

END MODULE
