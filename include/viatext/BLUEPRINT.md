namespace viatext {

CLASS ViaTextCore:
    // --- Internal State ---
    VARIABLE node_id : STRING                  // This node's ID (callsign)
    VARIABLE tick_count : INTEGER              // Number of ticks since startup
    VARIABLE uptime : INTEGER                  // System uptime in milliseconds
    VARIABLE last_timestamp : INTEGER          // Last tick timestamp in ms
    VARIABLE inbox : QUEUE<STRING>             // Incoming message argument strings
    VARIABLE outbox : QUEUE<STRING>            // Outbound message argument strings
    VARIABLE received_message_ids : SET<STRING> // Track received message IDs to prevent duplication

    // --- Initialization ---
    METHOD __init__(optional_node_id : STRING = ""):
        SET node_id = optional_node_id
        SET tick_count = 0
        SET uptime = 0
        SET last_timestamp = 0
        CLEAR inbox, outbox, received_message_ids

    // --- Add inbound message (raw arg string, as from CLI, LoRa, etc) ---
    METHOD add_message(arg_string : STRING):
        ENQUEUE inbox <- arg_string

    // --- Tick: update time, process one message per tick ---
    METHOD tick(current_timestamp : INTEGER):
        IF last_timestamp == 0:
            last_timestamp = current_timestamp
        uptime += (current_timestamp - last_timestamp)
        last_timestamp = current_timestamp
        tick_count += 1
        CALL process()

    // --- Main process loop: handles one message per tick ---
    METHOD process():
        IF inbox is EMPTY:
            RETURN
        arg_string = DEQUEUE inbox

        // --- Parse argument string ---
        parser = ArgParser(arg_string)
        msg_type = parser.get_message_type()

        // --- SWITCH TRACK: Select behavior based on message type ---
        SWITCH msg_type:
            CASE "-m":                  // Standard message
                CALL handle_message(parser)
            CASE "-p":                  // Ping request
                CALL handle_ping(parser)
            CASE "-mr":                 // Mesh report/request
                CALL handle_mesh_report(parser)
            CASE "-ack":                // Acknowledge
                CALL handle_acknowledge(parser)
            CASE "-save":               // Save message to storage (wrapper-specific)
                CALL handle_save(parser)
            CASE "-load":               // Load message from storage (wrapper-specific)
                CALL handle_load(parser)
            CASE "-get_time":           // Request time (e.g. for sync)
                CALL handle_get_time(parser)
            CASE "-set_time":           // Set current node/system time
                CALL handle_set_time(parser)
            CASE "-id":                 // Set node ID (via argument)
                CALL set_node_id(parser.get_arg("-id"))
            // --- ADD ADDITIONAL CASES AS NEEDED ---
            DEFAULT:
                // Unknown or unhandled argument; drop or log

    // --- Example handler for a standard message ---
    METHOD handle_message(parser : ArgParser):
        message = Message(parser.get_arg("-data"))
        // Deduplication: check message ID
        IF message.id.sequence IN received_message_ids:
            RETURN    // Already processed
        ADD message.id.sequence TO received_message_ids

        // Check if message is addressed to this node
        IF message.to == node_id:
            // Prepare response/ack if needed
            IF message.requests_acknowledgment():
                ack_args = build_ack_args(message)
                ENQUEUE outbox <- ack_args
            // Log or deliver to application (wrapper decides)
            delivered_args = build_received_args(message)
            ENQUEUE outbox <- delivered_args
        ELSE:
            // Not for us: maybe forward/rebroadcast (wrapper decides)

    // --- Handler for ping request ---
    METHOD handle_ping(parser : ArgParser):
        // Build ping reply args
        reply_args = "-pong -from " + node_id
        ENQUEUE outbox <- reply_args

    // --- Handler for mesh report request ---
    METHOD handle_mesh_report(parser : ArgParser):
        // Build and enqueue mesh report args
        mesh_args = "-mr -from " + node_id + " -neigh_count " + get_neighbor_count()
        ENQUEUE outbox <- mesh_args

    // --- Handler for acknowledge messages ---
    METHOD handle_acknowledge(parser : ArgParser):
        // Log or update state for ACK
        ack_id = parser.get_arg("-msg_id")
        // Application/wrapper handles display, removal, etc.

    // --- Handlers for save/load/get_time/set_time can be customized per wrapper ---

    // --- Utility: Set node ID from args or at init
    METHOD set_node_id(new_id : STRING):
        node_id = new_id

    // --- Utility: Retrieve and remove next outbound message (for wrapper)
    METHOD get_message() -> STRING:
        IF outbox is EMPTY:
            RETURN ""
        RETURN DEQUEUE outbox

    // --- Utility: Build args for reply/received/ack (use -data as needed)
    METHOD build_ack_args(message : Message) -> STRING:
        RETURN "-ack -from " + node_id + " -to " + message.from + " -msg_id " + message.id.sequence

    METHOD build_received_args(message : Message) -> STRING:
        RETURN "-r -from " + message.from + " -data " + message.to_wire_string()

    // --- Helper: Get neighbor count (stub for mesh logic) ---
    METHOD get_neighbor_count() -> INTEGER:
        RETURN 0   // Real implementation would track mesh peers

END CLASS

} // END namespace viatext
