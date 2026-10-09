"""Точка входа: мини-аналог u-center для u-blox M9."""
import sys

from PyQt6.QtWidgets import QApplication

from ucenter.ui.main_window import MainWindow
from ucenter.ui.style import apply_theme


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("UCenter")  # задаёт каталог дискового кэша тайлов
    app.setStyle("Fusion")  # одинаковая основа на macOS и Windows, остальное — в QSS
    apply_theme(app)
    win = MainWindow()
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
