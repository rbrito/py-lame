#!/usr/bin/env python

from distutils.core import setup
from distutils.extension import Extension

lame_module = Extension("lame",
			["lamemodule.c"],
			include_dirs = ["/usr/local/include"],
			library_dirs = ["/usr/local/lib"],
			libraries = ["mp3lame"])

setup(name = "py-lame",
      description = "Python interface to the LAME encoder.",
      version = "0.1",
      author = "Alexander Leidinger",
      author_email = "Alexander@Leidinger.net",
      maintainer = "Kyle VanderBeek",
      maintainer_email = "kylev.lame@kylev.com",
      url = "http://lame.sourceforge.net/",
      license = "BSD",
      ext_modules = [lame_module],
      scripts = ['slame'],
      data_files = [ ('share/docs/py-lame', ['README']) ],
      )
