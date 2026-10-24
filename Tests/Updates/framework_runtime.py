"""Match Unity Mono's Windows path handling in standalone Framework test hosts.

This is executable-local runtime configuration, never a machine policy change.
"""
from pathlib import Path


def configure_managed_test_driver(executable):
    Path(str(executable) + ".config").write_text(
        '<configuration><runtime><AppContextSwitchOverrides '
        'value="Switch.System.IO.UseLegacyPathHandling=false;Switch.System.IO.BlockLongPaths=false" />'
        '</runtime></configuration>', encoding="utf-8")
