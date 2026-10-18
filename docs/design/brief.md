# Design brief

Status: proposed. The author approves this brief before the theme and the screens.

This brief sets the look of Codicier. The theme tokens, the fonts, the icons, and each screen mock follow from it. If a later design choice does not fit the brief, change the brief first.

## Goal

Codicier must look like a well-made book on a quiet desk. The text of the author is the most important thing on the screen. Every other part of the app supports the text and stays calm.

## Principles

1. **The page comes first.** The writing view shows the text and little else. Panels, counts, and tools appear when the author asks for them.
2. **Book typography, not app typography.** The text uses a serif book face, a line of about 66 characters, and generous space between lines. The Markdown marks stay in the text, but they are quieter than the words.
3. **Paper and ink, not white and black.** The light theme is a warm paper colour with dark brown ink. The dark theme is a warm charcoal with soft ink. No theme uses pure white or pure black.
4. **One accent colour, used rarely.** The accent marks the current item, the focus, and footnote marks. It never fills a large area.
5. **Quiet planning tools.** The codex, the boards, and the timeline can show much information. They use clear alignment, rules, and space, not boxes with heavy shadows.
6. **Motion with a reason.** Motion shows where a thing comes from or goes to. Each move takes 150 to 250 ms with a soft ease. Nothing bounces. If the system asks for less motion, the app uses none.
7. **The keyboard first.** Each action has a key. The mouse is optional for writing.
8. **One system.** Each colour, size, space, radius, and duration comes from `Theme`. A screen never uses its own value.
9. **Readable for all.** Each text and background pair has a contrast ratio of 4.5 or more. The focus is always visible. The text size can change.

## Mood

The mood is a study at night: paper, ink, a desk lamp, and a closed door. The app feels calm, warm, and exact.

These words describe the mood:

- calm
- warm
- bookish
- exact
- quiet

## References

| Reference | What to take from it |
|---|---|
| Vellum | The book quality of the type, and the calm preview of a finished page. |
| iA Writer | The focus on the text, the styled Markdown with visible marks, and the small number of controls. |
| Scrivener | The binder and the inspector: the structure of a long manuscript, always one key away. |
| A printed novel | The measure, the margins, the running heads, and the scene breaks. |

## What to avoid

- The look of a web dashboard: cards, badges, and many colours.
- Glass effects, strong gradients, and neon colours.
- Heavy shadows and large rounded corners.
- Emoji in the interface.
- Default widget styles from a desktop environment.
- Pure white, pure black, and pure grey surfaces.

## Starting values from the spike

The spike in `spike/` already uses these values. Task T13 turns them into theme tokens and checks the contrast.

| Use | Value |
|---|---|
| Paper, light theme | `#fbf8f1` |
| Ink, light theme | `#2b2620` |
| Accent | `#9a5b2e` |
| Markdown marks | `#b5a996` |
| Reading font | Crimson Pro, 21 px |
| Line width | 66 characters |

The colour of the Markdown marks has a contrast ratio of 2.18 on the paper. The ink has 14.13, and the accent has 5.06. Task T13 must make it darker, or the author must accept an exception for the marks.

## Fonts

Task T14 picks the fonts. Each font must have an open licence, and the app bundles it.

| Role | Candidates |
|---|---|
| Reading | Crimson Pro, Literata, Source Serif 4, EB Garamond |
| Interface | iA Writer Quattro, Inter, IBM Plex Sans |

## Decisions for the author

1. Approve the mood: a study at night, warm paper and ink.
2. Pick the default theme on the first start: light, dark, or the system setting.
3. Pick the rule for the Markdown marks: the full contrast of 4.5, or a lower contrast as a recorded exception.
4. Add or remove a reference.
