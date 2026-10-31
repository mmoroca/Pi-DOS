# Preparing the first GitHub sync

Reviewed 30 October 2026. Destination: `https://github.com/rtomasa/Pi-DOS.git`.
The connected GitHub API reports a **public repository**, default branch `main`,
size zero, and an account with push permission. This is a read-only metadata
observation, not a completed Git transport/authentication test or a future
promise that the remote remains empty.

## Prepared source copy

Run from the original project root:

```sh
python3 tools/repo/prepare-github.py
```

This creates a standalone Git working repository at **`output/github-sync`**.
It exports the current branch's development history while excluding
`.vscode/pro-deployer.json` from **every commit**, then adds a preparation commit
containing the reviewed working-tree source/documentation. All 41 original
baseline development commits are retained in order, with their authors/messages
and normal history relationships; removing the deployment file changes commit
IDs. Original and resulting IDs are mapped in ignored
`output/github-sync-report.json`, alongside counts and verification results.

The source repository's HEAD, index, refs and local deployment configuration
are not modified. The generated copy is not an object-sharing clone: its Git
objects are imported without the excluded file. It has origin configured and
a clean committed working tree, but no upstream is claimed before an actual
push. No network operation, commit to the original repository, force push or
public upload is performed by the preparer.

The snapshot includes tracked working-tree files and an explicit small list of
new preparation files. It does not indiscriminately include untracked content.
Tracked deletions are reflected; symlinks/merge conflicts need review. Other
branches and tags are not exported. The existing `dl/`, build output, images,
user-supplied ROMs/SoundFonts and local account state stay outside publication.

Existing output directories are never overwritten. To regenerate after edits,
choose a fresh ignored output path:

```sh
python3 tools/repo/prepare-github.py --output output/github-sync-v2
```

The helper checks every reachable historical blob for a focused set of likely
credential patterns, rejects blobs of 50 MiB or more, checks the excluded path,
requires a clean resulting tree and runs Git object-integrity verification.
This focused check does not replace reviewing source and distribution rights.
No Git LFS migration is required by the inspected baseline's object sizes.

## Why the original working repository must not be pushed as-is

The original history contains a local editor SFTP configuration with a login
password. Adding it to `.gitignore`, deleting it in a later commit or using an
ordinary clone would leave the earlier blob reachable. The preparation tool
removes that path from the entire **separate publication history**, preserving
the local settings and original repository for development/reference.

A credential-free example is tracked at
[.vscode/pro-deployer.example.json](../.vscode/pro-deployer.example.json).
For a development Linux machine, copy it locally to `pro-deployer.json` and
configure authentication outside version control. This SFTP workflow targets
a development host; the Pi-DOS appliance itself has no SSH server.

The original repository still tracks its local file because this preparation
intentionally leaves its index/history intact. **Use the generated repository
for the first sync.** For subsequent development, continue from that clean copy
or a fresh GitHub clone after publication; do not merge the original credential-
containing history back into it. Preserve the original locally as needed.
If the credential was reused or previously shared, change it on that device.

## Remaining publication decisions

The repository's existing [license/provenance review](../LICENSES/README.md)
remains open. No license was selected on the author's behalf:

- Select a license for original Pi-DOS code; Buildroot currently marks the init
  package license as unknown. Retain upstream licenses and notices separately.
- Review the **tracked** ARJ shareware and personalized EDIT binaries before
  publishing them publicly, as well as the applicable notices/source for the
  other vendored components. Technical Git readiness is not permission to
  redistribute those binaries.
- Complete the broader component/firmware/game/demo/SoundFont/ROM review before
  uploading images, packs or release artifacts. ROM/SF2 assets are not in Git;
  generated images can contain those and other separately restricted payloads.

The prepared copy is ready for source review and first-sync mechanics; public
publication still depends on resolving the applicable rights decisions above.
If further edits are needed, make them in the original working source and
prepare a fresh copy, or commit them directly in the publication copy after
review. Do not upload `output/`, `dl/` or a bundle of the original Git history.

## First sync procedure after review

From the **prepared copy** (not the original repository):

```sh
cd output/github-sync
git status --short
git log --oneline --reverse
git remote -v
git ls-remote --heads --tags origin
```

Confirm that there is no remote history requiring reconciliation. The local
`origin/main: gone` status in the original repository only means its tracking
reference is missing; it is not a reason to run an initial `git pull`.
GitHub connector permission and shell Git authentication are separate. Configure
your normal HTTPS credential helper or SSH authentication if Git requests it;
never put a token in a URL or tracked configuration.

For a still-empty destination, after the content/license review:

```sh
git push --dry-run origin main
git push -u origin main
```

These are operator instructions, not actions already performed. Do not use
`--force`, `--mirror` or `--all`. If GitHub now contains an unrelated README,
license or other initial commit, fetch and inspect that history before choosing
how to integrate it; do not overwrite it or blindly allow unrelated histories.
A successful `push -u` establishes normal pull/push tracking. If you use a new
clone afterward, restore any required ignored local assets separately.

## Changes made for this preparation

- Added the complete [technical/manual reference](technical-reference.md),
  including every NOTES.md feature and all original development commits.
- Linked the reference and this sync guide from README and NOTES.
- Added line-ending/binary attributes for cross-platform source handling,
  preserving DOS batch CRLF, exact patch bytes (including mixed-line-ending
  context), and binary executable/archive/EDID payloads.
- Expanded ignore rules for local editor credentials, account/agent state and
  transient editor files; preserved existing download/build exclusions.
- Added a credential-free deployment example and a repeatable, local-only
  history export/preparation helper.

No runtime feature changed, hardware was flashed, expensive image build was
started, GitHub repository setting was changed or public push was performed.
