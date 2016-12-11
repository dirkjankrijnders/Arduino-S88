import sys
import os
import shlex
import subprocess

read_the_docs_build = os.environ.get('READTHEDOCS', None) == 'True'

if read_the_docs_build:
    subprocess.call('doxygen', shell=True)

extensions = ['breathe']
breathe_projects = { 'Arduino-S88': 'xml' }
breathe_default_project = "Arduino-S88"
templates_path = ['_templates']
source_suffix = '.rst'
master_doc = 'index'
project = u'Arduino-S88'
copyright = u'2015, Arduino-S88'
author = u'Arduino-S88'
version = '1.0'
release = '1.0'
language = None
exclude_patterns = ['_build']
pygments_style = 'sphinx'
todo_include_todos = False
html_static_path = ['_static']
htmlhelp_basename = 'Arduino-S88doc'
latex_elements = {
}
latex_documents = [
  (master_doc, 'Arduino-S88.tex', u'Arduino-S88 Documentation',
   u'Arduino-S88', 'manual'),
]
