# Pi-DOS 1.0 User's Guide — editable sources

This package contains the complete English user guide, its Typst layout and the small builder that keeps the Markdown and PDF editions in sync.

## Files

- `pi-dos-user-guide.md` — editable manuscript and canonical content.
- `pi-dos-user-guide.typ` — generated, self-contained Typst source; can be compiled directly.
- `pi-dos-user-guide.pdf` — compiled book (provided separately if you downloaded the sources ZIP).
- `build-guide.py` — Markdown-to-Typst conversion, shared layout and PDF compilation.
- `requirements.txt` — versions used for this edition.
- `fonts/` — Nimbus Roman and Nimbus Mono PS fonts and their notices.

The guide uses a 155 × 230 mm page, black-and-white typography, facing-page margins, indented body text, triangle-marked procedures, ruled running headers, Roman-numbered front matter, numbered chapters, repeated table headers, PDF bookmarks, linked contents and a linked subject index with resolved page numbers.

## Edit and rebuild

Use Python 3.10 or later. In an environment of your choice, install the two dependencies:

```sh
python -m pip install -r requirements.txt
python build-guide.py
```

The builder writes the Typst source and compiles the PDF beside the manuscript. Fonts are loaded from the included fonts directory. No Typst network packages, TeX installation or external service are needed for compilation once these dependencies are installed.

To regenerate only the Typst source:

```sh
python build-guide.py --source-only
```

If you prefer the Typst CLI, the builder can use it when the Python typst package is absent. You can also compile the generated source directly:

```sh
typst compile --font-path fonts pi-dos-user-guide.typ pi-dos-user-guide.pdf
```

This edition was built with Typst 0.15.0 and markdown-it-py 4.2.0. Other compiler versions may produce different page breaks.

## Where to make changes

Change prose and command examples in the Markdown. The HTML comments FRONTMATTER and MAINMATTER separate cover metadata, front matter and numbered chapters; retain them. PAGEBREAK comments provide deliberate print breaks in the glossary. Level-two headings start chapters or appendices, level-three headings are sections, and level-four headings become triangle-marked procedures. Fenced blocks become command/output samples. Standard Markdown tables become print tables with repeated headers. Internal links resolve to heading labels; print page references are added automatically.

Change shared typography, the cover and fixed edition metadata in the TEMPLATE constant near the start of `build-guide.py`. The generated `.typ` file can also be edited directly, but running the builder again replaces those direct edits. For a layout-only workflow, keep your edited Typst source and compile it directly.

If you rename a heading, update the corresponding Markdown index links. Chapter references in prose and tables are editorial text; review them if you reorder chapters. The builder supports the Markdown constructs used by this manuscript, rather than every Markdown extension.

A long command can wrap visually in the PDF at a backslash or semicolon. It remains one command line in the Markdown. For copying exact command text, prefer the Markdown source. Example paths assume the named file exists; replace them as appropriate.

## Editorial scope

The supplied technical-reference.md, reviewed 30 October 2026, is the behavioral source for Pi-DOS 1.0 at baseline ff0c194. The supplied 1993 MS-DOS concise guide informed organization and appearance. The user-facing prose is written for Pi-DOS rather than copied wholesale from that manual.

The guide does not claim new hardware acceptance testing. It preserves the supplied reference's distinction between implemented behavior, host checks and physical confirmation. Repository audit tables and the full development history remain in the technical reference, rather than being reproduced as user instructions. The book is in English; the documented Spanish keyboard/Spain/Europe-Madrid defaults describe Pi-DOS, not a presumed reader preference.

The original reference files and Pi-DOS repository were not modified.

## Fonts

The included fonts are unmodified Nimbus Roman and Nimbus Mono PS from URW Base35. See `fonts/NOTICE.txt` for copyright, the AGPL-3 license and the document embedding exception. Upstream source: https://github.com/ArtifexSoftware/urw-base35-fonts . Font licensing does not select a license for the Pi-DOS project or manuscript.

## Print and screen use

Use actual size when printing to matching 155 × 230 mm paper. For A4 or Letter, use your viewer's booklet/imposition or fit settings as appropriate. The PDF is a reader edition with facing-page margins, not a printer-imposed signature file. Contents, bookmarks and index links support navigation on screen.
