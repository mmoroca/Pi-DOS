#!/usr/bin/env python3
"""Regenerate Typst from the Markdown manuscript, then compile the PDF.
Dependencies: Python 3.10+, markdown-it-py; either typst Python package or typst CLI.
"""
from pathlib import Path
import argparse
import json
import re
import shutil
import subprocess
from markdown_it import MarkdownIt

ROOT = Path(__file__).resolve().parent
STEM = 'pi-dos-user-guide'

TEMPLATE = r'''// Generated from pi-dos-user-guide.md by build-guide.py.
// Edit the Markdown for content; edit TEMPLATE in the builder for shared styling.
#set document(title: "Pi-DOS 1.0 User's Guide", author: "Pi-DOS", date: datetime(year: 2026, month: 10, day: 30))
#set text(font: "Nimbus Roman", size: 10.5pt, lang: "en", hyphenate: true)
#set par(justify: true, leading: 4.6pt, first-line-indent: 0pt, spacing: 5.5pt)
#set page(width: 155mm, height: 230mm, margin: (inside: 20mm, outside: 17mm, top: 21mm, bottom: 18mm), binding: left, header: none, footer: none)
#set heading(numbering: none)
#set enum(indent: 0pt, body-indent: 5mm, spacing: 4pt)
#set list(indent: 0pt, body-indent: 4mm, spacing: 4pt)
#set table(inset: (x: 3.2pt, y: 4.2pt), stroke: none)
#let running = state("running", "Contents")
#let p(body, sticky: false) = block(inset: (left: 10mm), above: 0pt, below: 5.5pt, sticky: sticky, body)
#let inline-code(s) = text(font: "Nimbus Mono PS", size: 0.87em, hyphenate: false, s)
#let code-block(s) = block(inset: (left: 10mm), above: 2pt, below: 8pt)[
  #set par(justify: false, leading: 2.5pt)
  #text(font: "Nimbus Mono PS", size: 9.3pt, weight: "bold", hyphenate: false, s)
]
#let procedure(body) = block(sticky: true, above: 7pt, below: 4pt)[
  #grid(columns: (10mm, 1fr), align: (left, left), [#h(3mm)#polygon(fill: black, stroke: none, (0pt, 0pt), (6pt, 3.5pt), (0pt, 7pt))], [#text(weight: "bold", body)])
]
#let notice(body) = block(inset: (left: 10mm), above: 4pt, below: 8pt)[
  #set par(justify: true)
  #body
]
#let items(body) = block(inset: (left: 10mm), above: 0pt, below: 6pt, breakable: false, body)
#let reference-table(cols, cells, compact: false) = block(above: 3pt, below: 9pt, breakable: cells.len() > 14)[
  #set text(size: if compact { 9pt } else { 9.4pt })
  #set table(inset: (x: 3.2pt, y: if compact { 3pt } else { 4.2pt }))
  #set par(justify: false, leading: 3.5pt)
  #table(columns: cols, table.header(..cells.slice(0, cols.len()).map(c => table.cell(fill: luma(94%), text(weight: "bold", c)))), table.hline(stroke: 0.45pt), ..cells.slice(cols.len()), table.hline(stroke: 0.45pt))
]
#show heading.where(level: 1): it => {
  running.update(it.body)
  block(above: 5mm, below: 7mm, sticky: true)[
    #set text(size: 24pt, weight: "bold", hyphenate: false)
    #set par(justify: false, leading: 3pt)
    #it.body
  ]
}
#show heading.where(level: 2): it => block(above: 9pt, below: 5pt, sticky: true)[
  #set text(size: 16pt, weight: "bold", hyphenate: false)
  #set par(justify: false, leading: 3.5pt)
  #it.body
]
#show link: set text(fill: black)
#let book-header() = context {
  let n = counter(page).get().first()
  let page-label = counter(page).display()
  let current-chapters = query(heading.where(level: 1)).filter(h => h.location().page() == here().page())
  let title = if current-chapters.len() > 0 { current-chapters.first().body } else { running.get() }
  set text(size: 8.6pt, weight: "bold", hyphenate: false)
  set par(justify: false)
  if calc.even(n) {
    grid(columns: (8mm, 1fr), page-label, [Pi-DOS 1.0 User's Guide])
  } else {
    grid(columns: (1fr, 8mm), align(right, title), align(right, page-label))
  }
  v(2pt)
  line(length: 100%, stroke: 0.45pt)
}

// Cover: deliberately monochrome, with the generous spacing of a printed manual.
#v(9mm)
#text(size: 16pt, weight: "bold", hyphenate: false)[User's Guide]
#v(25mm)
#text(size: 46pt, weight: "bold")[Pi-DOS]
#v(3mm)
#text(size: 20pt)[Version 1.0]
#v(7mm)
#line(length: 100%, stroke: 1.1pt)
#v(5mm)
#text(size: 14pt)[A DOS-compatible computer\ for Raspberry Pi]
#v(1fr)
#text(size: 10pt)[Edition 1\ 30 October 2026]
#pagebreak()

// Verso / edition note.
#v(12mm)
#text(size: 17pt, weight: "bold")[Pi-DOS 1.0 User's Guide]
#v(6mm)
#text(size: 10pt)[Edition 1 · 30 October 2026\ Technical baseline: ff0c194]
#v(8mm)
#text(size: 10pt)[Prepared from the supplied Pi-DOS technical reference. This edition describes the reviewed implementation; it does not claim new physical hardware validation.]
#v(4mm)
#text(size: 10pt)[Names of example programs and files are illustrative. Games, commercial software and some MIDI assets must be supplied separately where required.]
#v(4mm)
#text(size: 10pt)[The organization and page design are inspired by the 1993 Microsoft MS-DOS 6 Concise User's Guide. The prose and procedures are written for Pi-DOS. Microsoft endorsement is not implied.]
#v(4mm)
#text(size: 10pt)[Component and asset licenses remain independent. No project-wide license is selected by this guide. See Appendix D and the project's notices.]
#v(1fr)
#line(length: 100%, stroke: 0.45pt)
#v(3mm)
#text(size: 9pt)[155 × 230 mm · Monochrome edition]
#pagebreak()

// Contents and front matter use Roman numbering; the chapters restart at 1.
#counter(page).update(3)
#set page(numbering: "i", header: book-header(), footer: none)
#running.update([Contents])
#text(size: 24pt, weight: "bold", hyphenate: false)[Contents]
#v(6mm)
#{
  set text(size: 9.5pt)
  set par(justify: false, leading: 3.5pt)
  show outline.entry: it => {
    if it.level == 1 { v(4pt); strong(it) } else { it }
  }
  outline(title: none, depth: 2, indent: 5mm)
}
'''


def s(value):
    return json.dumps(value, ensure_ascii=False)


def slug(value):
    return re.sub(r'[^\w\- ]', '', value.lower()).replace(' ', '-')


def inline(tok):
    out = []
    for t in tok.children or []:
        if t.type == 'text':
            out.append('#text(' + s(t.content) + ')')
        elif t.type == 'code_inline':
            out.append('#inline-code(' + s(t.content) + ')')
        elif t.type == 'strong_open': out.append('#strong[')
        elif t.type == 'em_open': out.append('#emph[')
        elif t.type in ('strong_close', 'em_close'): out.append(']')
        elif t.type == 'softbreak': out.append(' ')
        elif t.type == 'hardbreak': out.append('#linebreak()')
        elif t.type == 'link_open':
            href = t.attrGet('href')
            if href.startswith('#'):
                out.append('#link(label(' + s('h-' + href[1:]) + '))[')
            else:
                out.append('#link(' + s(href) + ')[')
        elif t.type == 'link_close':
            out.append(']')
            # Index links carry an automatically resolved print page reference.
            opens = [c for c in (tok.children or [])[:(tok.children or []).index(t)] if c.type == 'link_open']
            if opens and opens[-1].attrGet('href').startswith('#'):
                target = 'h-' + opens[-1].attrGet('href')[1:]
                out.append('#text(" (p. ")#ref(label(' + s(target) + '), form: "page", supplement: none)#text(")")')
        else:
            raise ValueError('Unsupported inline token: ' + t.type)
    return ''.join(out)


def render(tokens):
    out, i = [], 0
    while i < len(tokens):
        t = tokens[i]
        if t.type == 'heading_open':
            body = inline(tokens[i+1])
            level = int(t.tag[1:])
            plain = tokens[i+1].content
            if level == 1:
                pass  # The cover is typeset explicitly.
            elif level == 2:
                if plain.startswith('Chapter ') or plain.startswith('Appendix '):
                    out.append('#pagebreak(weak: true)\n')
                else:
                    out.append('#pagebreak(weak: true)\n')
                out.append('#heading(level: 1)[' + body + ']#label(' + s('h-' + slug(plain)) + ')\n')
            elif level == 3:
                out.append('#heading(level: 2)[' + body + ']#label(' + s('h-' + slug(plain)) + ')\n')
            elif level == 4:
                out.append('#procedure[' + body + ']\n')
            i += 3
        elif t.type == 'paragraph_open':
            # Numbered/list paragraphs get their own spacing from the list container.
            out.append(('#p(sticky: true)[' if tokens[i+1].content.rstrip().endswith(':') else '#p[') + inline(tokens[i+1]) + ']\n')
            i += 3
        elif t.type in ('fence', 'code_block'):
            text = t.content.rstrip('\n')
            # Soft wrapping opportunities preserve one command line visually.
            text = re.sub(r'(?<=[;\\])(?=\S)', '\u200b', text)
            out.append('#code-block(' + s(text) + ')\n')
            i += 1
        elif t.type == 'html_block':
            if '<!-- PAGEBREAK -->' in t.content:
                out.append('#pagebreak()\n')
            if '<!-- MAINMATTER -->' in t.content:
                out.append('#pagebreak(to: "odd")\n#counter(page).update(1)\n#set page(numbering: "1", header: book-header())\n')
            i += 1
        elif t.type in ('ordered_list_open','bullet_list_open'):
            ordered = t.type == 'ordered_list_open'
            close = 'ordered_list_close' if ordered else 'bullet_list_close'
            depth, j = 1, i+1
            entries = []
            current = []
            while j < len(tokens):
                q = tokens[j]
                if q.type == t.type: depth += 1
                elif q.type == close:
                    depth -= 1
                    if depth == 0: break
                if q.type == 'list_item_open' and depth == 1:
                    current = []
                elif q.type == 'list_item_close' and depth == 1:
                    entries.append(current)
                else: current.append(q)
                j += 1
            rendered = []
            for entry in entries:
                # Tight lists here contain a single paragraph per entry.
                if len(entry) == 3 and entry[0].type == 'paragraph_open':
                    rendered.append('[' + inline(entry[1]) + ']')
                else:
                    rendered.append('[' + render(entry) + ']')
            name = 'enum' if ordered else 'list'
            start = ('start: ' + str(t.attrGet('start') or 1) + ', ') if ordered else ''
            out.append('#items[#' + name + '(' + start + ', '.join(rendered) + ')]\n')
            i = j+1
        elif t.type == 'blockquote_open':
            j, depth = i+1, 1
            while j < len(tokens):
                if tokens[j].type == 'blockquote_open': depth += 1
                if tokens[j].type == 'blockquote_close':
                    depth -= 1
                    if depth == 0: break
                j += 1
            quoted = tokens[i+1:j]
            if len(quoted) == 3 and quoted[0].type == 'paragraph_open':
                body = inline(quoted[1])
            else: body = render(quoted)
            out.append('#notice[' + body + ']\n')
            i = j+1
        elif t.type == 'table_open':
            j = i+1
            cells, header = [], []
            while tokens[j].type != 'table_close':
                if tokens[j].type in ('th_open','td_open'):
                    cells.append('[' + inline(tokens[j+1]) + ']')
                    if tokens[j].type == 'th_open': header.append(tokens[j+1].content)
                j += 1
            n = len(header)
            first = header[0].lower() if header else ''
            ratio = '(0.9fr, 1.1fr)' if first in ('command','path','shortcut','default shortcut') else '(0.85fr, 1.15fr)'
            if n != 2: ratio = '(' + ', '.join(['1fr']*n) + ')'
            out.append('#reference-table(' + ratio + ', (\n' + ',\n'.join(cells) + ',\n), compact: ' + ('true' if first == 'subject' else 'false') + ')\n')
            i = j+1
        else:
            raise ValueError(f'Unsupported block token {t.type} at {i}')
    return '\n'.join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source-only', action='store_true', help='Regenerate .typ without compiling')
    args = ap.parse_args()
    md = (ROOT / (STEM + '.md')).read_text()
    # The cover metadata is kept in Markdown for stand-alone reading, but emitted explicitly in Typst.
    body = md.split('<!-- FRONTMATTER -->',1)[1]
    parser = MarkdownIt('commonmark').enable('table')
    source = TEMPLATE + '\n' + render(parser.parse(body))
    typ = ROOT / (STEM + '.typ')
    typ.write_text(source)
    if args.source_only:
        print(typ)
        return
    pdf = ROOT / (STEM + '.pdf')
    try:
        import typst
    except ImportError:
        cli = shutil.which('typst')
        if not cli:
            raise SystemExit('Install the typst Python package or Typst CLI to compile.')
        subprocess.run([cli, 'compile', '--font-path', str(ROOT / 'fonts'), str(typ), str(pdf)], check=True)
    else:
        _, warnings = typst.Compiler(typ, root=ROOT, font_paths=[ROOT / 'fonts']).compile_with_warnings(output=pdf)
        for warning in warnings:
            print('WARNING:', warning.diagnostic)
    print(pdf)

if __name__ == '__main__':
    main()
