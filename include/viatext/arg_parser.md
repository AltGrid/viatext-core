# arg_parser.BLUEPRINT.md

MODULE ArgParser

  ## DOC
  ## @description: MCU-safe, heap-free argument tokenizer for the ViaText protocol.
  ## @role: Tokenizes shell-style input into directive, flags, and key-value pairs.
  ## @compatibility: ESP32, AVR, Linux, LoRa — must use fixed memory, ETL containers.
  ## @usage: Used after all input in Core and wrappers (serial, LoRa, CLI).
  ## @limitations: No quoting, escaping, or grouped/long-form flags.

  # --- Data Types

  TYPE Token: String    # Fixed-size, max 32 chars

  TYPE ArgParser {
    directive: Token,          # First token, e.g. "-m"
    flags: List<Token>,        # Standalone flags (e.g. "-ping")
    arguments: Map<Token, Token>, # Key-value pairs (e.g. "-rssi" → "92")
  } END TYPE

  # --- Constants
  CONST TOKEN_SIZE: Number = 32
  CONST MAX_FLAGS: Number = 8
  CONST MAX_ARGS: Number = 12
  CONST MAX_TOKENS: Number = 16
  CONST TAIL_KEYS: List<Token> = List("-data")  # Only these absorb remainder

  # --- Main Algorithm

  ## DOC
  ## @description: Parse a TextFragments stream into directive, flags, arguments.
  ## @param fragments: TextFragments — fragmented char stream, space-separated.
  ## @effect: Populates self.directive, self.flags, self.arguments.
  DEFINE parse(fragments: Object): Void
    CALL fragments.reset_character_iterator()
    SET tokens: List<Token> = List()

    # Phase 1: Tokenize (space-delimited, up to MAX_TOKENS)
    WHILE TRUE
      SET tok: Token = ""
      # Skip whitespace
      DO
        SET c: String = CALL fragments.get_next_character()
      WHILE c == " "
      IF c == 0
        BREAK
      END IF
      # Build token
      DO
        IF Length(tok) < TOKEN_SIZE
          SET tok = tok + c
        END IF
        SET c: String = CALL fragments.get_next_character()
      WHILE c != 0 AND c != " "
      IF tok != "" AND Length(tokens) < MAX_TOKENS
        CALL ListAppend(tokens, tok)
      END IF
      IF c == 0
        BREAK
      END IF
    END WHILE

    # Phase 2: Directive
    IF Length(tokens) > 0
      SET self.directive = tokens[0]
      SET idx: Number = 1
    ELSE
      SET self.directive = ""
      RETURN
    END IF

    # Phase 3: Main parse loop (flags, args, tail keys)
    WHILE idx < Length(tokens)
      SET key: Token = tokens[idx]
      SET is_tail: Boolean = CALL ListContains(TAIL_KEYS, key)
      IF is_tail
        # Tail key: consume rest as value (joined with space)
        SET rest: Token = ""
        FOR EACH j IN Range(idx+1, Length(tokens)-1)
          IF Length(rest) + Length(tokens[j]) + 1 < TOKEN_SIZE
            IF j > idx+1
              SET rest = rest + " "
            END IF
            SET rest = rest + tokens[j]
          END IF
        END FOR
        CALL MapSet(self.arguments, key, rest)
        BREAK
      END IF

      # Standard key-value: next token is value (if not dash-prefixed)
      IF idx+1 < Length(tokens) AND Substr(tokens[idx+1], 0, 1) != "-"
        CALL MapSet(self.arguments, key, tokens[idx+1])
        SET idx = idx + 2
      ELSE
        # Standalone flag
        IF Length(self.flags) < MAX_FLAGS
          CALL ListAppend(self.flags, key)
        END IF
        SET idx = idx + 1
      END IF
    END WHILE
  END DEFINE

  # --- API

  ## DOC
  ## @description: Check if flag is present.
  DEFINE has_flag(flag: Token): Boolean
    RETURN CALL ListContains(self.flags, flag)
  END DEFINE

  ## DOC
  ## @description: Check if key-value argument is present.
  DEFINE has_argument(key: Token): Boolean
    RETURN CALL MapHasKey(self.arguments, key)
  END DEFINE

  ## DOC
  ## @description: Get value for argument key.
  DEFINE get_argument(key: Token): Token
    IF CALL MapHasKey(self.arguments, key)
      RETURN CALL MapGet(self.arguments, key)
    ELSE
      RETURN ""
    END IF
  END DEFINE

END MODULE
