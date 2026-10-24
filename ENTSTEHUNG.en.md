# How it came about

It started as a model test. I wanted to see how far a local model running on my two RTX 3090s could get with a larger project, and a procedural graphics demo seemed like a worthy subject. One of the two cards has since died; Strata runs fast enough on a single one. I worked with the coding agent [Pi](https://pi.dev), first using Huihui-Qwen3.8-27B-abliterated (EXL3, 4.0 bpw) via TabbyAPI, and from 6 October on with Qwen 3.8 Flash Next (IQ3_S) via the [Strata](https://github.com/Niko1221/Strata) inference engine.

**Phase 1, 3 to 7 October: ten sessions.** Terrain, grass with fade-in and fade-out, font and HUD, first trees. With Pi's own compaction, each session went through two to four compactions, each at around 242,000 tokens. Every one of them meant waiting, and every pass lost more. At some point the model was only going round in circles; the context was poisoned. I then had it write the current state into `STATUS.md` and started a new session. That is also why `STATUS.md` is so long.

Surely this could be done better. The result is [pi-protokoll](https://github.com/sebion7125/pi-protokoll): the model writes a short protocol at the end of every turn, while it still has the whole turn in front of it. At compaction time, code assembles a tiered summary from these protocols instead of having the model summarise the entire history again, and every earlier turn stays retrievable through tools.

**Phase 2, from the evening of 7 October: a single session.** At the same time the port of [EZ-Tree](https://github.com/dgreenheck/ez-tree) began, i.e. the whole tree system with foliage tiers, transitions and wind. This session ran with the extension all the way to the publication of the repo: 179 messages, 23 compactions, each in about 30 seconds instead of minutes. A new session was never needed again.

The `protocol*.txt` files are exports of these protocols from phase 2.

---

*German version: [ENTSTEHUNG.md](ENTSTEHUNG.md)*
