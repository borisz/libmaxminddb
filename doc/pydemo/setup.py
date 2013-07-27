from distutils.core import setup, Extension

module1 = Extension('TMMDB',
	libraries = ['tinymmdb'],
	sources = ['py_libtinymmdb.c'],
	library_dirs = ['/usr/local/lib'],
	include_dirs = ['/usr/local/include'])

setup (name = 'TMMDB-Python',
	version = '1.0.0',
	description = 'This is a python wrapper to libtinymmdb',
	ext_modules = [module1])
