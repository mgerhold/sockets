# Basic configuration
project = 'Sockets'
copyright = '2024, Michael Gerhold'
author = 'Michael Gerhold'

# Extensions to use
extensions = [ "breathe", "exhale" ]

# Configuration for the breathe extension
# Which directory to read the Doxygen output from
breathe_projects = {"Sockets":"xml"}
breathe_default_project = "Sockets"

# Configuration for the theme
html_theme = "sphinx_rtd_theme"

exhale_args = {
    "containmentFolder": "./api",
    "doxygenStripFromPath": "../src",
    "rootFileName": "library_root.rst",
    "rootFileTitle": "Library API",
}