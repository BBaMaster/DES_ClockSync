// -----------------------------------------------------------------------------
// THESIS TEMPLATE FOR UAS TECHNIKUM WIEN
// Author: M. Horauer
// GITHUB: https://github.com/mhorauer/
// LICENSE: GPL-3.0-or-later
// -----------------------------------------------------------------------------

#import "uastw-thesis-lib.typ": *
#import "@preview/cmarker:0.1.1": render

// -----------------------------------------------------------------------------
// ---[ Adjust the variables below ]--------------------------------------------

//#let lan = "de"
#let lan = "en"

#let title    = "Multi Node Clock Synchronization"
#let subTitle = ""

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
// #show "Rust": rust

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

// Just in case - reset the counter for Headings ...
//
#counter(heading).update(0)
// =============================================================================
// --[ ADJUST YOUR CONTENT FILES BY ADDING/MODIFYING SECTIONS ]-----------------
//
// #include "sections/10_section1.typ"
// #include "sections/11_section2.typ"
// #include "sections//12_section3.typ"
// ...

#render(read("../introduction.md"))
#pagebreak()
#render(read("../../CLAUDE.md"))
#pagebreak()
#set heading(numbering: none)
#render(read("../../drs_architecture_reviewed_fixed_v_21.md"))
#set heading(numbering: "1.1")
#pagebreak()
#render(read("../telemetry_requirements.md"))
#pagebreak()
#render(read("../../telemetry_format.md"))
#pagebreak()
#render(read("sections/setup_2_nodes.md"))
#pagebreak()
#render(read("sections/results.md"))

// -----------------------------------------------------------------------------
// ---[ BIBLIOGRAPHY ]----------------------------------------------------------
// #bibliography(
// 	title: if lan == "en" [Bibliography] else [Literaturverzeichnis],
// 	"sections/90_works.bib"
// )
// -----------------------------------------------------------------------------
// --[ INDEX ]------------------------------------------------------------------
// #include "sections/99_index.typ"

// EOF
