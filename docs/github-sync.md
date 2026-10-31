# GitHub synchronization

Updated 31 October 2026. The single working repository is:

```text
/home/rtomas/Projects/Pi-DOS
```

Open this folder in VS Code and run all normal build and Git commands here.
There is no separate publication checkout under `output/`. Build artifacts,
downloads, generated guide PDF and local settings remain in their existing
locations and are ignored as configured in `.gitignore`.

## Repository history

The active Git metadata comes from the cleaned history that was prepared for
GitHub. It preserves all 41 original development commits, with the local
`.vscode/pro-deployer.json` deployment file excluded from every commit, followed
by the documentation/preparation commit and subsequent work. Removing that file
changes the development commit IDs; the subjects and order remain intact.
The original hashes in the technical reference identify the pre-cleanup history.

The earlier temporary `output/github-sync` and `output/github-sync-v2` checkouts
and their reports have been retired. The export helper was also removed: new
work is committed directly in the main project folder. Before consolidation,
a private recovery archive of the original Git metadata, temporary repositories
and reports was saved outside the project under `/tmp/pi-dos-consolidation-*`.
Temporary storage may be cleared; retain that archive elsewhere privately if
long-term recovery is needed. It includes the old credential-containing history
and must not be published.

Your local `.vscode/pro-deployer.json` stays on disk, is ignored and is absent
from the active history. The credential-free
[deployment example](../.vscode/pro-deployer.example.json) remains tracked.
It describes a development Linux host; the Pi-DOS appliance has no SSH server.

## Normal workflow

From the project root:

```sh
cd /home/rtomas/Projects/Pi-DOS
git status
git diff
```

Review and stage the specific source/documentation paths you changed, then
commit and push normally:

```sh
git add path/to/changed-file
git diff --cached
git commit -m "Describe the change"
git push
```

`main` tracks `origin/main` at `https://github.com/rtomasa/Pi-DOS.git`.
Consolidation preserves that remote configuration and tracking state; it does
not push, pull, force-push or change GitHub settings. A non-fast-forward rejection
requires inspection and reconciliation rather than an automatic force push.
Use `git pull --ff-only` when the working tree is clean and you want to receive
remote updates without creating an implicit merge commit.

## User guide

The editable guide project is tracked under `docs/pi-dos-user-guide`, including
the Markdown manuscript, Typst source, Python builder, dependencies, fonts and
notices. Its generated PDF is ignored. Follow
[the guide's build instructions](pi-dos-user-guide/README.md) to regenerate it.
New guide edits can be staged and committed directly from this repository.

## Publication and release decisions

The existing [license/provenance review](../LICENSES/README.md) remains separate
from Git synchronization. No license has been selected on the author's behalf:

- Select a license for original Pi-DOS code; the init package currently records
  its license as unknown. Retain component licenses/notices separately.
- Review the tracked ARJ shareware and personalized EDIT binaries and the
  applicable conditions for the other vendored components before public
  redistribution.
- Complete the broader component, firmware, game, demo, SoundFont and ROM review
  before distributing images or packs. ROM/SF2 assets are excluded from Git;
  generated images may contain separately restricted payloads.

The repository's ignore rules keep local account/agent/editor state, downloads
and build outputs out of routine source commits. They do not grant distribution
rights or remove a file that is already tracked. Inspect staged content before
committing new assets.
