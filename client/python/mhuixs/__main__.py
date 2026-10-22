"""``python -m mhuixs`` 等价于 ``mhuixs-cli``。"""

from .cli import cli

if __name__ == "__main__":
    cli()
