# BLUEPRINTS.md

## Blueprint.psd (Blueprint Pseudocode) — AI-Focused Pseudocode Blueprinting Language

### Purpose

Blueprint.psd is a language-agnostic, block-structured pseudocode system for specifying data, logic, and control flow **for AI and human use**.  
Its primary goal is to be losslessly interpreted by LLMs, codegen tools, and humans, without reference to any particular programming language.

**Design Priorities:**
- **AI Comprehension First:** No ambiguity; optimized for automated parsing, transformation, and intent extraction.
- **Human Readability Second:** Fully explicit, with readable comments and structure.
- **Universality:** No language-native tricks—only universal logic and types.

---

## 1. Core Syntax Principles

- All control blocks **require** explicit end markers (`END ...`).
- **All strings** are **double-quoted**: `"like this"`.
- **All identifiers** (variable names, keys) are **never quoted**.
- **No dynamic or language-native features** (no list comprehensions, lambdas, etc).
- **All docstrings** are structured with `## DOC` and @tags, directly before the entity.
- **No implicit conversions**; all types are explicit and must match.
- **Operators allowed:** `==`, `!=`, `<`, `>`, `<=`, `>=`, `+`, `-`, `*`, `/`, `%`, `AND`, `OR`, `NOT`.

---

## 2. Naming Conventions

- **Variables & Functions:** `snake_case`
- **Types & Enums:** `PascalCase`
- **Constants:** `UPPER_SNAKE_CASE`
- **String literals:** Always `"double quoted"`

---

## 3. Data Types & Structures

- `String` — Unicode text
- `Number` — Integer or float
- `Boolean` — `true` or `false`
- `List<T>` — List of T
- `Map<K, V>` — Map from K to V
- `Object` — Unstructured, generic
- `NULL` — Explicit null value
- **Optionals:** `T | NULL` (e.g., `email: String | NULL`)

### Type Declaration Example

```blueprint
TYPE User {
  id: Number,
  username: String,
  email: String | NULL
} END TYPE
```

---

## 4. Keyword Reference & Block Structure

### Declaration Blocks
- `TYPE ... END TYPE`
- `ENUM ... END ENUM`
- `MODULE ... END MODULE`
- `DEFINE [ASYNC] ... END DEFINE`
- `IMPORT ...` (for external modules)

### Actions & Flow
- `SET var_name: Type = value`
- `CALL function(args...)`
- `RETURN value`
- `IF ... END IF`
- `ELSE IF ...`
- `ELSE ...`
- `MATCH ... END MATCH`
- `FOR EACH ... END FOR`
- `PARALLEL FOR EACH ... END FOR`
- `WHILE ... END WHILE`
- `TRY ... CATCH ... FINALLY ... END TRY`
- `AWAIT CALL function(args...)` (async only)
- `ACTION GET / SAVE / UPDATE / DELETE ...`
- `LOG "message"`

---

## 5. List & Map Usage

- **Initialization:**  
  `SET users: List<User> = List()`
- **Appending to a List:**  
  `CALL ListAppend(users, user_profile)`
- **Map Set/Get:**  
  `CALL MapSet(response, "user", user_profile)`  
  `SET user = CALL MapGet(response, "user")`

---

## 6. Comments & Documentation

- **Single-line comment:** `# This is a comment`
- **Docstring for all functions/types:**  
  Begins with `## DOC` and uses `@` tags.

**Docstring Example:**
```blueprint
## DOC
## @description: Fetch user profile and posts by user ID.
## @param user_id: Number — The ID of the user to fetch.
## @return: Map<String, Object> — Contains user and posts.
```

---

## 7. Syntax Rules & Best Practices

- **All blocks require explicit end markers** (`END TYPE`, `END DEFINE`, etc).
- **String literals are always quoted** (`"..."`). Identifiers never are.
- **Each assignment includes type:** `SET count: Number = 5`
- **No implicit conversions:** Types must match as written.
- **No inline host language code** (SQL, JS, etc).
- **Function modifiers (e.g., ASYNC) go directly after DEFINE.**
- **Optionals/unions:** Use `| NULL`, not `?` or `Optional<T>`.

---

## 8. Example Blueprint (Full)

```blueprint
# User data structure
TYPE User {
  id: Number,
  username: String,
  email: String,
  is_active: Boolean,
  last_login: String | NULL
} END TYPE

# Enum for roles
ENUM UserRole {
  ADMIN = "admin",
  EDITOR = "editor",
  VIEWER = "viewer"
} END ENUM

MODULE BlogAPI

  ## DOC
  ## @description: Fetch a user's profile and their posts.
  ## @param user_id: Number — The ID of the user.
  ## @return: Map<String, Object> — User info and posts.
  DEFINE ASYNC fetch_user_data(user_id: Number): Map<String, Object>
    TRY
      SET user_profile: User | NULL = AWAIT CALL GetUserById(user_id)
    CATCH error
      LOG "Error fetching user: ${error.message}"
      RETURN NULL
    END TRY

    IF user_profile == NULL
      LOG "User not found: ${user_id}"
      RETURN NULL
    END IF

    SET user_posts: List<Object> = AWAIT CALL GetPostsByUserId(user_id)
    SET response: Map<String, Object> = Map()
    CALL MapSet(response, "user", user_profile)
    CALL MapSet(response, "posts", user_posts)

    RETURN response
  END DEFINE

END MODULE
```

---

## 9. Do’s & Don’ts

**Do:**
- End every block with `END ...`.
- Document every function/type with `## DOC` and `@tags`.
- Quote all string literals, never identifiers.
- Use only specified operators and keywords.

**Don’t:**
- Don’t use any language-native tricks or syntax.
- Don’t overload functions or use dynamic features.
- Don’t embed real code (SQL, JS, etc) in actions.

---

## 10. BNF Grammar (Partial)

```
<blueprint> ::= (<statement> | <block>)*
<block> ::= "TYPE" <identifier> "{" <fields> "}" "END TYPE"
         | "ENUM" <identifier> "{" <enum_fields> "}" "END ENUM"
         | "MODULE" <identifier> (<statement> | <block>)* "END MODULE"
         | "DEFINE" ["ASYNC"] <identifier> "(" <params> ")" ":" <type> <statements> "END DEFINE"
<statement> ::= "SET" <identifier> ":" <type> "=" <value>
             | "CALL" <identifier> "(" <args> ")"
             | "RETURN" <value>
             | <control_flow>
             | <comment>
<comment> ::= "#" <text>
<value> ::= <string> | <number> | <boolean> | "NULL" | <identifier> | <call>
<string> ::= '"' <chars> '"'
```

---

**End of Specification**
