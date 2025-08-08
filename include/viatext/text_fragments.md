# text_fragments.BLUEPRINT.md

MODULE TextFragments

  ## DOC
  ## @description: Fixed-size, heap-free fragmenter for tokenized messaging; breaks input strings into N equal ETL-safe chunks for microcontroller and Linux compatibility.
  ## @role: Core of all ViaText input parsing and payload handling. Enables deterministic, safe storage of user or protocol text in MCU/embedded environments.
  ## @integration: Used by ArgParser, Message, Core, CLI, LoRa, etc.
  ## @author: Leo, ChatGPT

  # --- Parameters & Types

  ## DOC
  ## @description: Template parameters.
  CONST MaxFragments: Number = 8     # Default: number of fragments
  CONST FragSize: Number = 32        # Default: size of each fragment

  TYPE TextFragments<MaxFragments, FragSize> {
    fragments: List<String(FragSize)>,         # Array of up to MaxFragments ETL strings (fixed-capacity)
    used_fragments: Number,                    # How many are filled (≤ MaxFragments)
    error: Number,                             # 0=OK, 1=Overflow, 2=Empty/cleared
    iter_idx: Number,                          # For next() fragment iteration
    get_next_character_index: Number,          # Char position within current fragment
    get_next_character_fragment_index: Number, # Current fragment index for char streaming
    character_iteration_complete: Boolean       # Linear character stream finished
  } END TYPE

  ENUM TextFragmentsError {
    OK = 0,
    OVERFLOW = 1,
    EMPTY = 2
  } END ENUM

  # --- Constructors

  ## DOC
  ## @description: Create empty fragment container (error=EMPTY).
  DEFINE new(): TextFragments
    RETURN TextFragments(
      fragments = List(String(FragSize), MaxFragments),
      used_fragments = 0,
      error = 2,
      iter_idx = 0,
      get_next_character_index = 0,
      get_next_character_fragment_index = 0,
      character_iteration_complete = false
    )
  END DEFINE

  ## DOC
  ## @description: Construct and immediately split src string into fragments.
  DEFINE from_string(src: String): TextFragments
    SET tf = TextFragments.new()
    CALL tf.set(src)
    RETURN tf
  END DEFINE

  # --- Core Methods

  ## DOC
  ## @description: Split src into up to MaxFragments fixed-size chunks; sets error if overflow or empty.
  DEFINE set(self: TextFragments, src: String): Void
    # Clear previous; set used_fragments, error
    # For each fragment:
    #   - Copy up to FragSize chars from src
    #   - Fill up to MaxFragments
    # If leftover chars after MaxFragments: error=OVERFLOW
    # If input empty: error=EMPTY
  END DEFINE

  ## DOC
  ## @description: Reset fragment iterator for next().
  DEFINE reset(self: TextFragments): Void
    self.iter_idx = 0
  END DEFINE

  ## DOC
  ## @description: Return pointer to next fragment, or NULL if finished.
  DEFINE next(self: TextFragments): String | NULL
    IF self.iter_idx < self.used_fragments
      SET frag = self.fragments[self.iter_idx]
      self.iter_idx = self.iter_idx + 1
      RETURN frag
    ELSE
      RETURN NULL
    END IF
  END DEFINE

  ## DOC
  ## @description: Return number of filled fragments.
  DEFINE count(self: TextFragments): Number
    RETURN self.used_fragments
  END DEFINE

  ## DOC
  ## @description: Safe indexed access.
  DEFINE at(self: TextFragments, idx: Number): String
    RETURN self.fragments[idx]
  END DEFINE

  ## DOC
  ## @description: Erase all fragments, reset state to empty.
  DEFINE clear(self: TextFragments): Void
    FOR EACH i IN Range(0, MaxFragments-1)
      self.fragments[i] = ""
    END FOR
    self.used_fragments = 0
    self.error = 2
    self.iter_idx = 0
    self.get_next_character_index = 0
    self.get_next_character_fragment_index = 0
    self.character_iteration_complete = false
  END DEFINE

  ## DOC
  ## @description: Return next char in linear stream across fragments, or 0 if complete. Sets character_iteration_complete.
  DEFINE get_next_character(self: TextFragments): Char
    IF self.character_iteration_complete OR self.used_fragments == 0
      RETURN 0
    END IF
    IF self.get_next_character_index >= Length(self.fragments[self.get_next_character_fragment_index])
      self.get_next_character_fragment_index = self.get_next_character_fragment_index + 1
      self.get_next_character_index = 0
      IF self.get_next_character_fragment_index >= self.used_fragments
        self.get_next_character_index = 0
        self.get_next_character_fragment_index = 0
        self.character_iteration_complete = true
        RETURN 0
      END IF
    END IF
    SET c = self.fragments[self.get_next_character_fragment_index][self.get_next_character_index]
    self.get_next_character_index = self.get_next_character_index + 1
    RETURN c
  END DEFINE

  ## DOC
  ## @description: Reset character stream iterator.
  DEFINE reset_character_iterator(self: TextFragments): Void
    self.get_next_character_index = 0
    self.get_next_character_fragment_index = 0
    self.character_iteration_complete = false
  END DEFINE

END MODULE
