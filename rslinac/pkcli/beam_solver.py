# -*- coding: utf-8 -*-
u"""Run pyhellweg library.

:copyright: Copyright (c) 2016 RadiaSoft LLC.  All Rights Reserved.
:license: http://www.apache.org/licenses/LICENSE-2.0.html
"""
from __future__ import absolute_import, division, print_function
from pyhellweg import PyHellwegCppException
import os
import pyhellweg
import six
import sys


def run(ini_file, input_file, output_file):
    """Call `pyhellweg.run_beam_solver` with inputs and write result.

    Read `ini_file` and `input_file`, execute the beam solver, and write
    result to `output_file`. Arguments may be `py.path.local` objects
    or strings. Input files must exist, and output file must not exist.

    Args:
        ini_file (object): path to configuration settings in INI format
        input_file (object): path to input data
        output_file (object): path to write the result

    Raises:
        PyHellwegCppException: if an error occurs within the Beam Solver
    """
    # TODO(elventear) Provide links that document the input formats
    pyhellweg.run_beam_solver(
        _run_arg(ini_file),
        _run_arg(input_file),
        _run_arg(output_file, False),
    )


def _run_arg(a, assert_readable=True):
    """Encode file names and ensure readable/not exist."""
    a = str(a)
    if isinstance(a, six.text_type):
        a = a.encode()
    if assert_readable:
        if os.access(a, os.R_OK):
            return a
        e = 'does not exist or not readable'
    else:
        if not os.path.exists(a):
            return a
        e = 'already exists'
    pkcli.command_error('{}: {}', a, e)
