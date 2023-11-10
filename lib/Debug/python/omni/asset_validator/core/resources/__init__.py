from pathlib import Path

BASIC_TUTORIAL_PATH: str = Path(__file__).parent.joinpath("tutorial.usda").as_posix()
"""A path to the usda used in the on boarding tutorial."""

LAYERS_TUTORIAL_PATH: str = Path(__file__).parent.joinpath("layers_tutorial.usda").as_posix()
"""A path to the usda used in the on boarding tutorial."""
