// -----------------------------------------------------------------------------
// THESIS TEMPLATE FOR UAS TECHNIKUM WIEN
// Author: M. Horauer
// GITHUB: https://github.com/mhorauer/
// LICENSE: GPL-3.0-or-later
// -----------------------------------------------------------------------------

#import "uastw-thesis-lib.typ": *
#import "@preview/cmarker:0.1.8": render as cmarker-render
#import "@preview/mitex:0.2.7": mitex
#let render(body) = cmarker-render(body, math: mitex)

// -----------------------------------------------------------------------------
// ---[ Adjust the variables below ]--------------------------------------------

//#let lan = "de"
#let lan = "en"

#let title    = "Multi Node Clock Synchronization"
#let subTitle = "Condensed Lab Report"

#let authors = (
  (name: "Jakob-Elias Frenzel", id: "es24m016"),
  (name: "Bernhard Bauer", id: "es24m013"),
)

#let course = "Distributed Embedded Systems"
#let loc    = "Wien"

// --- OUTPUT THE TITLEPAGE ----------------------------------------------------
#set page(numbering: none)
#show: uastw-thesis-titlepage.with(
	language: lan,
    thesis-type: "LAB REPORT",
    degree: "",
    study-program: "",
    thesis-title: title,
    thesis-subtitle: subTitle,
    author: authors.map(a => a.name).join(", "),
    authorid: authors.map(a => a.id).join(", "),
    advisor1: course,
    advisor2: "",
    location: loc)

// --- SETUP THE PAGE STYLING & SOME VARIABLES ---------------------------------
#show: uastw-thesis-page-setup
#show "LaTeX": latex
#show "BibTeX": bibtex

// --- WE START WITH PAGE NUMBERING @KURZFASSUNG -------------------------------
#set page(footer: context [
	#set text(twgray, size: 10pt)
	#align(right, counter(page).display("1"))
	])
#set page(numbering: "1")

// --- INSERT TABLE OF CONTENTS ------------------------------------------------
#outline(
	title: if lan == "en" [Table of Contents] else [Inhaltsverzeichnis]
)

#counter(heading).update(0)
#set heading(numbering: "1.1")

// =============================================================================
// --[ CONDENSED CONTENT FLOW ]-------------------------------------------------
// =============================================================================

#render(read("../introduction.md"))
#pagebreak()

#render(read("../../CLAUDE.md"))
#pagebreak()

#render(read("sections/implementation_deviations.md"))
#pagebreak()

#render(read("sections/visualizer_and_fixes.md"))
#v(1.5em)
#align(center)[
  #figure(
    image("sections/images/visualizer_screenshot.png", width: 90%),
    caption: [Real-time DRS Cluster Visualizer dashboard showing 2 active synchronized nodes.]
  ) <fig-visualizer>
]
#pagebreak()

#render(read("sections/setup_2_nodes.md"))
#pagebreak()

#render(read("sections/results.md"))

// EOF
