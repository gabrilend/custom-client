# 810 — Spell Description Text

*Written 2026-10-16 from the clean-room checklist's section 7
([docs/057](../docs/057-what-the-original-program-decides.md)). The spell
table holds text full of tokens (`$s1`, `$d`, `$lpoint:points;`); the
original program turns them into the sentence a tooltip shows. Nothing
in the data says how.*

## Blocked by

- [106](./106-dbc-parser.md) (the spell, duration, radius, range and
  description-variable tables).
- For the parts that read the player (stats, auras, known spells, gear):
  Phase 5 and the player's state in Phase 8.

## Blocks

- Every tooltip that shows a spell or aura (Phase 7's interface,
  [808](./808-phase-demo.md)).

## Current behavior

Nothing exists. The tables are readable (106); no text is built from
them.

## Intended behavior

Given a spell (and, for an aura, the unit it is on), the client builds
the same description text the original shows, character for character,
in each of the original's languages.

- **Reading** (note 7.1): which text (description or aura description);
  description variables first; copy up to each `$`; scale, spell number
  and effect digit before and after the letter; a token that can't be
  read becomes a literal `$` and the text is marked failed but still
  shown.
- **Each letter's value and format** (note 7.2, one table row each),
  with "the last number" kept for plurals.
- **Durations in words** (row "numbers, durations and money", filled
  2026-10-16): only the largest unit (days from 1 day, hours from 1
  hour, minutes from 1 minute, else seconds); the number cut down, or
  rounded up where the caller asks, with a rounded-up 24 hours shown as
  1 day and 60 minutes as 1 hour; seconds always cut down. The words are
  the interface's strings, the caller's name plus `_DAYS`, `_HOURS`,
  `_MIN` or `_SEC` (such as `INT_SPELL_DURATION_HOURS`), so each
  language's wording and plurals come from its own data. Money and
  other number formats wait on docs/057.
- **Gender** `$g` (note 7.3), **plural** `$l` with the per-language rules
  for English, German, Spanish, Korean, French, Chinese and Russian (note
  7.4).
- **Conditions** `$?` with `&` and `|` grouping to the right, **formulas**
  `${}` with their functions and names, and **description variables**
  (note 7.5).

### Quirks, each with a correction switch (off by default: the original)

From docs/057 note 7.6. Each switch is a setting the player can turn on,
and both choices are tested.

| Quirk | The original | The correction |
|---|---|---|
| 7.6.1 | effect digits 4 to 9 read effect 1 | treat them as a bad token |
| 7.6.2 | the misc value inside a formula is always 0 | read the misc value |
| 7.6.3 | `A % B` gives A rounded down to a multiple of B | give the remainder |
| 7.6.4 | a leading minus binds before `^` (`-2^2` is 4) | none: data may rely on it; built as written, no switch |
| 7.6.5 | `$r` shows the range cut down but picks the plural rounded up | (none given in docs/057) |
| 7.6.6 | `$t` decides "whole" in the period's largest unit (90 s prints "90.00") | test whole seconds |

7.6.2 and 7.6.3 may be slips in the decompilation rather than the
original (docs/057 open question 6). They are built as quirks like the
rest. If the decompilation's upstream later fixes them, docs/057's
answer changes, and so does this table.

## Suggested implementation steps

1. The reader: the token parts, the letter table, the literal-`$`
   failure.
2. `$g`, `$l` with the language rules, `$?` conditions, `${}` formulas,
   description variables.
3. The correction switches in the settings store.
4. A test table of spells, each with the expected text for a fixed
   player:
   - a duration of 90 s shows as minutes in `$d`, and as "90.00" in `$t`
     (and "90" with the correction);
   - `$lpoint:points;` after 1 and after 0 in English (singular, then
     plural), and after 21, 22 and 25 in Russian (forms 1, 2, 3);
   - `a1&a2|s3` read as a1 and (a2 or s3);
   - `$s4` shows effect 1's amount, and a literal `$` with the correction;
   - a range of 5.5 yards shows "5" with the plural chosen for 6;
   - 5,400 seconds shows as 1 hour, and as 2 hours when rounding up;
     86,000 seconds rounded up shows 1 day.
5. Run over every spell in the table: count the texts marked failed and
   write the count to the run's goodbye receipt, so a change that breaks
   reading shows up as a number.

## Open questions

None.

## Answered

1. ~~**Building the two doubtful quirks.**~~ As quirks, original first,
   with their correction switches (2026-10-16). Asked how a wrong number
   from a copying mistake would be found, the owner: "because the decomp
   project's upstream fixed it..."

## Related

- [docs/057](../docs/057-what-the-original-program-decides.md) section 7,
  [106](./106-dbc-parser.md).
