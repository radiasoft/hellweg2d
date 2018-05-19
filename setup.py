# -*- coding: utf-8 -*-
"""rslinac setup script

:copyright: Copyright (c) 2016 RadiaSoft LLC.  All Rights Reserved.
:license: http://www.apache.org/licenses/LICENSE-2.0.html
"""
from glob import glob
from setuptools import Extension
import os
import sysconfig
import pykern

_LIB_DIR = os.path.join('src', 'libHellweg2D')

_PHYS_DIR = os.path.join('src', 'physics')


def _get_src_files(directory, extension):
    """Get list of files in `directory` with `extension`"""
    return glob(os.path.join(directory, '*.{}'.format(extension)))


def _get_compile_args():
    """Returns compiler arguments based on $CC or Python's default CC"""
    compiler = os.environ.get('CC', sysconfig.get_config_var('CC'))
    # libHellweg2D uses c++11 features
    if 'gcc' in compiler:
        return ['-std=c++11']
    return None


pksetup.setup(
    name='rslinac',
    author='RadiaSoft LLC',
    author_email='pip@radiasoft.net',
    description='description',
    license='http://www.apache.org/licenses/LICENSE-2.0.html',
    url='https://github.com/radiasoft/rslinac',
    classifiers=[
        'Development Status :: 2 - Pre-Alpha',
        'Environment :: Console',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: Apache Software License',
        'Natural Language :: English',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python',
        'Topic :: Utilities',
    ],
    ext_modules=[
        Extension(
            name='rslinac.pyhellweg',
            define_macros=[('RSLINAC', 1)],
            include_dirs=[_LIB_DIR, _PHYS_DIR],
            language='c++',
            sources=_get_src_files(_PHYS_DIR, 'cpp') +
                _get_src_files(_LIB_DIR, 'cpp') +
                ['pyhellweg.cpp'],
            extra_compile_args=_get_compile_args(),
        ),
    ],
)
