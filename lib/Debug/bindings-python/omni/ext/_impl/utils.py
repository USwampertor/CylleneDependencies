from functools import wraps
from time import process_time

import carb

_info_exec_time_indent = 0


def log_exec_time(operation_name=None):
    def _print_exec_time(f):
        @wraps(f)
        def wrap(*args, **kw):
            global _info_exec_time_indent
            ts = process_time()
            _info_exec_time_indent += 1
            result = f(*args, **kw)
            te = process_time()
            name = operation_name if operation_name else f"func:{f.__name__}"
            carb.log_info("{}> {} took: {:2.4f}s".format("-" * _info_exec_time_indent, name, te - ts))
            _info_exec_time_indent -= 1
            return result

        return wrap

    return _print_exec_time
