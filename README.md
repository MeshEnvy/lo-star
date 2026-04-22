# lo-star

**lo-star** is a small, portable **PlatformIO library** (“Universal Mesh API”) shared across Meshtastic, MeshCore, and related firmware. Use it when you want **one codebase** for settings, protobuf helpers, logging, messaging, and filesystem helpers that can compile in more than one upstream tree.

This repository is MIT-licensed; see [LICENSE](LICENSE).

## Cross-platform projects with lo-star

A cross-platform setup usually has three layers:

1. **lo-star** — shared types, codecs, and utilities with minimal coupling to any one radio stack.
2. **Your product library** (for example a `lotato/` library next to the firmware) — ingest, HTTP, and protocol mapping that still avoids hard-coding one SDK where possible; split RTOS or Arduino specifics into small **platform** or **provider** units.
3. **Each firmware fork** — build flags, includes, registration, and **thin delegates** into (2); avoid duplicating business logic in the fork tree.

Keep **HAL, build glue, and registration** in the firmware repo; keep **behavior you want identical across targets** in lo-star and your library. That way Meshtastic and MeshCore stay aligned without merging unrelated trees.

## Delegate pattern (thin fork, fat library)

Upstream radio and module code should not grow large product-specific branches. Prefer **delegation**:

- In the fork, at the natural hook (radio event, module tick, BLE callback, etc.), call **one or two functions** on your side (`lotato::meshcore::Provider::…`, `lotato::meshtastic::Provider::…`, or your own namespace).
- Implement the real work in your **library** or under `providers/`, where you can unit test and reuse code.

Example shape (conceptual):

```cpp
// In the firmware fork — minimal
void onSomeRadioEvent() {
  myproject::Provider::onRadioEvent(ctx, event);
}
```

```cpp
// In your portable library — full behavior
void myproject::Provider::onRadioEvent(Context& ctx, const RadioEvent& event) {
  // framing, potato-mesh payloads, state, etc.
}
```

If a change does not strictly need to live in the fork for **linking, includes, or platform HAL**, keep it out of the fork.

## Adding lo-star as a submodule (per firmware)

Each firmware repository that should compile against lo-star needs its **own submodule pointer** (or equivalent vendored copy). From the root of that firmware repo:

```bash
git submodule add https://github.com/MeshEnvy/lo-star.git lo-star
git submodule update --init --recursive
```

If you use SSH remotes internally, use `git@github.com:MeshEnvy/lo-star.git` instead.

Wire it into **PlatformIO** with a path dependency, for example in the consuming library’s `library.json`:

```json
{
  "dependencies": [
    { "name": "lo-star", "version": "symlink://../lo-star" }
  ]
}
```

Adjust the relative path so it resolves from your library’s directory to the submodule checkout. After cloning a parent repo, always run:

```bash
git submodule update --init --recursive
```

so `lo-star` (and nested deps such as Nanopb) are present for CI and local builds.

## Critical: Nanopb compile flag consistency

`lo-star` uses nanopb with:

```text
PB_FIELD_32BIT=1
```

This define must be applied consistently to:

- your app/library sources that include generated `*.pb.h`
- the nanopb runtime sources (`pb_encode.c`, `pb_decode.c`, `pb_common.c`)

If only one side gets the define, protobuf decode can silently return wrong values (for example `kind=0`, empty bytes) even though files look valid on disk.

In PlatformIO, put `-D PB_FIELD_32BIT=1` in shared env `build_flags` so both application code and dependencies are compiled with the same setting.

## Project branches based on the latest upstream tag

Treat **upstream tags** as stable bases. For each firmware fork:

1. Add the **original** firmware remote as `upstream` (if you forked MeshEnvy’s Lotato-ready forks, add both the true upstream and your fork as needed).
2. Fetch tags: `git fetch upstream --tags`
3. List tags and pick the release you want to track (for example the newest `v2.7.*` on Meshtastic or `repeater-v*` / `companion-v*` on MeshCore — **check the tags in your fork**; naming differs by project).
4. Create your long-lived branch from that tag:

```bash
git checkout -b myproject-upstream-v2.7.21 tags/v2.7.21
```

Naming is yours; many teams use **`<product>-<upstream-short>`** so history stays readable. Rebase **private** experiments if you like; for anything others build from (MeshForge users, submodule pins), prefer **merge** from new upstream tags rather than rewriting published history.

## Release tag naming: `<product>-<version>-<base-firmware>-<version>`

Use tags so a user can see **your product version** and **which upstream firmware** it was built against without opening commits. A practical convention:

```text
<product>-<version>-<base-firmware>-<version>
```

Examples (illustrative only — **always compare with real tags** on your fork and on [MeshEnvy](https://github.com/MeshEnvy)):

- `acme-1.0.0-meshtastic-2.7.21`
- `acme-1.0.0-meshcore-repeater-1.9.1`

Use lowercase product slugs, semver (or your scheme) for your side, and a short **unambiguous** upstream label (`meshtastic`, `meshcore`, role names, etc.). If you publish on MeshForge, consistent tags also pair well with optional `meshforge.yaml` regex filters (see below).

## Forking upstream and making your branch the default

1. **Fork** the upstream or MeshEnvy firmware repository on GitHub into your org or user.
2. Clone your fork, add `upstream` pointing at the source you intend to merge from, and create your **project branch** from the tag you chose (previous section).
3. Push your branch: `git push -u origin myproject-upstream-v2.7.21`
4. On GitHub: **Settings → General → Default branch** → set your project branch as default.

Why it matters:

- Collaborators and CI clone the branch you actually maintain.
- **MeshForge** reads `meshforge.yaml` from the **default branch** when filtering tags and build targets.

Keep merging new **upstream release tags** into your default branch so your tags stay honest about the base revision.

## Distributing builds on MeshForge

[MeshForge](https://meshforge.org) cloud-builds and web-flashes **PlatformIO** projects. Any public GitHub repo with a `platformio.ini` can be opened by swapping the host:

| GitHub | MeshForge |
|--------|-----------|
| `https://github.com/YourOrg/your-firmware` | `https://meshforge.org/YourOrg/your-firmware` |

Users pick a **tag or ref** and a **PlatformIO environment** (`[env:…]`). Optional **`meshforge.yaml`** at the repo root (on the **default branch**) narrows tag and target lists; see the upstream guide: [MeshEnvy/mesh-forge **DEVELOPER.md**](https://github.com/MeshEnvy/mesh-forge/blob/main/DEVELOPER.md).

Practical checklist:

- **Default branch** = branch where you merge releases and commit `meshforge.yaml`.
- Push **release tags** with clear names (see above).
- In the MeshForge UI, use **Refresh tags** after pushing tags or changing discovery metadata.
- Ensure submodules (including **lo-star**) are **committed** at the pins you want; MeshForge clones recursively like a normal CI checkout.

## Further reading

- [Mesh Forge README](https://github.com/MeshEnvy/mesh-forge/blob/main/README.md) — user-facing overview.
- [Mesh Forge DEVELOPER.md](https://github.com/MeshEnvy/mesh-forge/blob/main/DEVELOPER.md) — `meshforge.yaml`, tag filters, capabilities.
- Lotato / fork boundaries (delegate pattern in more detail) — `.cursor/rules/lotato-architecture.mdc` in the Lotato aggregate repo, if you have it checked out.
