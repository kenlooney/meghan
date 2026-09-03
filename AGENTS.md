# AGENTS.md — Meghan Compiler Episode Transcripts

## Project context

This repo holds **Meghan** (the `meg` compiler), a compiler-construction project by Kenneth Looney, dedicated to his niece. The journey is documented as an episode series. Each episode transcript in `transcripts/` is written as a **source script that is fed to Windows Copilot to generate an AI podcast**. The transcript is not documentation — it is spoken narration in Markdown form.

Start with `transcripts/intro.md` as the tone reference. It worked perfectly; match it.

## Your job

The journey is documented **only** in the episode transcripts under `transcripts/`. `README.md` stays strictly technical.

Only write a transcript when Kenneth explicitly asks for one. Do not generate, update, or suggest transcripts on your own just because code changed.

When asked to generate an episode transcript:

1. Ask (or infer from the repo state) what actually happened in that episode — what was built, what broke, what was learned.
2. Write `transcripts/<episode-name>.md` following the structure and voice rules below.
3. Do not invent technical progress that did not happen. Ground the content in the real code/commits in this workspace.

## File conventions

- Location: `transcripts/`
- Naming: lowercase, hyphenated, no numbers padding — `intro.md`, `episode-1.md`, `episode-2.md`.
- Format: Markdown, first-person, present/near-future tense.
- One `#` H1 title at the top, then `##` sections.

## Voice rules (the part that matters most)

Think of each episode as a **personal dev journey log kept for a listener or reader** — not a diary for himself, and not a spec. It is honest and informal like a journal entry, but it always explains things in plain language and always points at what comes next, because someone is on the other end. When a rule below does not cover an edge case, reason from that.

The intro worked because it sounds like a person talking, not a spec. Preserve these traits:

- **First person, conversational.** "I", "you", contractions. Speak to the listener directly.
- **Short paragraphs.** One to three sentences. This is read aloud — long blocks sound flat.
- **Enthusiastic but humble.** Kenneth openly admits what he doesn't know yet ("I will have a lot to learn before this"). Keep that honesty; it's the charm of the series.
- **Section headers are questions or hooks**, not nouns. Use things like:
  - "You ask why another compiler project?"
  - "Why now?"
  - "What to expect!"
  - "What is next?"
- **Emphasis carries the read.** Use `*italics*` on terms being introduced (`*transpiler*`, `*tokenizer*`) and `**bold**` on names and key ideas (`**meg**`).
- **Exclamation points are allowed** and welcome — sparingly, at moments of genuine excitement.
- **Personal stakes stay present.** The niece, the "for the fun of it" motivation, and the low-level curiosity are the heart of the series. Reference them when natural — never force it into every episode.
- **Warm sign-off.** Every episode ends with a short, friendly closing line pointing at the next episode. Example energy: "I will try to share episodes on my journey so take care I will see you very soon!"

### Do not

- Do not write in third person or a neutral documentation tone.
- Do not use bullet-heavy structure as a substitute for narration. Bullets are for short lists only (like backend targets), never for the main story.
- Do not add emojis.
- Do not pad with filler, marketing language, or "in this episode we will explore..." boilerplate.
- Do not include large code dumps. Code appears only as tiny illustrative snippets.

## Episode skeleton

Adapt, don't follow rigidly:

```markdown
# <Episode title>

<One-paragraph cold open: who I am / where we left off / what today is about.>

## <Hook question about the main topic>
<The core of the episode — what was built and why.>

## <How it works / what I learned>
<Plain-language explanation. Assume the listener is curious but not a compiler expert.>

## What went wrong
<Honest account of bugs, dead ends, or things that confused me. Optional but strong.>

## What is next?
<One short paragraph teasing the next episode.>

<Warm sign-off line.>
```

## Code snippets

Keep them tiny and readable aloud-adjacent. Use the `meg` language tag for Meghan source and pipeline diagrams:

````markdown
```meg
[meg source] -> [C code] -> [machine code]
```
````

Use `c` for generated C output. If a snippet is longer than roughly ten lines, describe it in prose instead.

## Technical facts to stay consistent with

- The compiler is named **Meghan**, referred to as **meg**.
- It starts life as a *transpiler* targeting **C**, then to machine code.
- Long-term goal: real backends for **freestanding** and **hosted** (Windows, Linux) environments.
- License: **Apache 2.0**.
- Episode 1 covers project setup and the first tokenizer — the intro teased "maybe more", so anything extra that genuinely got built that session belongs there too.

If a later episode changes any of these, update this list.

## README.md is strictly technical

`README.md` documents the project itself — what **meg** is, how to build it, how to use it, how to contribute. It is not part of the episode series.

- Do not add episode summaries, transcript links, or podcast narration to `README.md`.
- Do not write `README.md` in the transcript voice. It stays neutral, technical documentation.
- Generating or editing a transcript never implies a `README.md` change, and vice versa.
- The story of the journey lives in `transcripts/`, and nowhere else.
