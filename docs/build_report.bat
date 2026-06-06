@echo off
cd /d "%~dp0\.."
typst compile --root . docs/report/report.typ docs/report/report.pdf
