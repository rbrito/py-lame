#!/usr/bin/env python

from distutils.core import setup
from distutils.extension import Extension

pylame_version = '0.1'
pylame_version_str = '"%s"' % (pylame_version,)

lame_module = Extension('_lame',
                        ['lamemodule.c'],
                        define_macros=[('PYLAME_VERSION', pylame_version_str)],
                        include_dirs=['/usr/local/include'],
                        library_dirs=['/usr/local/lib'],
                        libraries=['mp3lame'],
                        )

setup(name='py-lame',
      description='Python interface to the LAME encoder.',
      version=pylame_version,
      author='Alexander Leidinger',
      author_email='Alexander@Leidinger.net',
      maintainer='Kyle VanderBeek',
      maintainer_email='kylev@kylev.com',
      url='http://lame.sourceforge.net/',
      license='BSD',
      ext_modules=[lame_module],
      py_modules=['lame'],
      scripts=['slame'],
      data_files=[('share/docs/py-lame', ['README'])],
      )
