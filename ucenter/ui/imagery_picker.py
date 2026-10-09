"""Выбор подложки карты (спутник Esri/Google или OSM); выбор запоминается между запусками."""
from PyQt6.QtCore import QSettings, pyqtSignal
from PyQt6.QtWidgets import QComboBox

from ucenter.ui.tile_loader import ESRI_IMAGERY, GOOGLE_IMAGERY, OSM, TileSource

SETTING = "azimuth/map_source"
# (подпись, источник тайлов); первый — по умолчанию
CHOICES = (
    ("Satellite — Esri", ESRI_IMAGERY),
    ("Satellite — Google", GOOGLE_IMAGERY),
    ("Map — OpenStreetMap", OSM),
)


class ImageryPicker(QComboBox):
    source_changed = pyqtSignal(object)  # TileSource

    def __init__(self, parent=None):
        super().__init__(parent)
        for label, source in CHOICES:
            self.addItem(label, source.cache_dir)
        self.setToolTip("Imagery provider. If tiles stay empty, the provider\n"
                        "may be blocked on this network — pick another one.")
        saved = QSettings().value(SETTING, CHOICES[0][1].cache_dir)
        self.setCurrentIndex(max(0, self.findData(saved)))
        self.currentIndexChanged.connect(self._on_changed)

    def source(self) -> TileSource:
        return CHOICES[self.currentIndex()][1]

    def _on_changed(self, _index: int):
        QSettings().setValue(SETTING, self.currentData())
        self.source_changed.emit(self.source())
