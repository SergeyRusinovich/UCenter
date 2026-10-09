"""Тесты логики без GUI: геодезия, измерение A->B, хранение, поиск мест, правила точек.

Запуск: python -m unittest discover -s tests
"""
import os
import tempfile
import unittest

from ucenter.core.baseline import format_deviation, measure
from ucenter.core.baseline_store import load_json, save_json, write_csv
from ucenter.core.geo import inverse
from ucenter.core.places import merge_duplicates, parse_nominatim, sort_by_distance
from ucenter.core.points import MeasurementPoints


def dms(d, m, s):
    return d + m / 60 + s / 3600


class GeodesyTest(unittest.TestCase):
    def test_vincenty_reference(self):
        # Пример Geoscience Australia: Flinders Peak -> Buninyong
        dist, fwd, back = inverse(-dms(37, 57, 3.72030), dms(144, 25, 29.52440),
                                  -dms(37, 39, 10.15610), dms(143, 55, 35.38390))
        self.assertAlmostEqual(dist, 54972.271, places=3)
        self.assertAlmostEqual(fwd, dms(306, 52, 5.37), places=5)
        self.assertAlmostEqual(back, dms(127, 10, 25.07), places=5)

    def test_same_point_has_no_azimuth(self):
        self.assertEqual(inverse(55.75, 37.62, 55.75, 37.62), (0.0, None, None))

    def test_near_antipodal_falls_back_to_sphere(self):
        dist, fwd, _ = inverse(0, 0, 0.5, 179.7)
        self.assertGreater(dist, 19_000_000)
        self.assertIsNotNone(fwd)


class BaselineTest(unittest.TestCase):
    def test_due_north(self):
        m = measure((55.75, 37.62), (55.76, 37.62))
        self.assertAlmostEqual(m.azimuth, 0.0, places=6)
        self.assertAlmostEqual(m.d_east, 0.0, places=6)
        self.assertAlmostEqual(m.d_north, m.distance_m, places=6)

    def test_deviation_sign(self):
        west = measure((55.75, 37.62), (55.76, 37.60))
        self.assertLess(west.deviation, 0)
        self.assertTrue(format_deviation(west.deviation).endswith("(W)"))
        self.assertTrue(format_deviation(measure((55.75, 37.62), (55.76, 37.64)).deviation).endswith("(E)"))


class StoreTest(unittest.TestCase):
    def test_json_roundtrip_and_csv(self):
        items = [measure((55.75, 37.62), (55.76, 37.63))]
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "sub", "m.json")
            self.assertEqual(load_json(path), [])  # файла ещё нет
            save_json(path, items)
            self.assertEqual(load_json(path), items)
            csv_path = os.path.join(tmp, "m.csv")
            write_csv(csv_path, items)
            with open(csv_path, encoding="utf-8") as fh:
                self.assertEqual(len(fh.read().strip().splitlines()), 2)


def _item(name, address, lat, lon, geo):
    return {"name": name, "display_name": address, "lat": str(lat), "lon": str(lon),
            "boundingbox": [str(lat - 0.01), str(lat + 0.01), str(lon - 0.01), str(lon + 0.01)],
            "geojson": geo}


class PlacesTest(unittest.TestCase):
    def test_parse_geometry_kinds(self):
        line = {"type": "LineString", "coordinates": [[37.6, 55.7], [37.61, 55.71]]}
        poly = {"type": "Polygon", "coordinates": [[[37.6, 55.7], [37.61, 55.7], [37.6, 55.71]]]}
        places = parse_nominatim([_item("a", "a, X", 55.7, 37.6, line),
                                  _item("b", "b, X", 55.7, 37.6, poly),
                                  _item("c", "c, X", 55.7, 37.6, {"type": "Point"}),
                                  {"broken": True}])
        self.assertEqual([(len(p.areas), len(p.lines)) for p in places], [(0, 1), (1, 0), (0, 0)])
        self.assertEqual(places[0].lines[0][0], (55.7, 37.6))  # GeoJSON [lon, lat] -> (lat, lon)

    def test_merge_street_segments(self):
        line = {"type": "LineString", "coordinates": [[37.6, 55.7], [37.61, 55.71]]}
        places = parse_nominatim([
            _item("Тверская улица", "Тверская улица, 58, Москва, 125009", 55.76, 37.60, line),
            _item("Тверская улица", "Тверская улица, 57, Москва, 103265", 55.77, 37.61, line),
            _item("Тверская улица", "Тверская улица, Казань", 55.79, 49.10, line),
        ])
        merged = merge_duplicates(places)
        self.assertEqual(len(merged), 2)
        self.assertEqual(len(merged[0].lines), 2)

    def test_sort_by_distance(self):
        far = parse_nominatim([_item("far", "far", 55.79, 49.10, None)])[0]
        near = parse_nominatim([_item("near", "near", 55.76, 37.60, None)])[0]
        self.assertEqual([p.name for p in sort_by_distance([far, near], (55.75, 37.62))], ["near", "far"])


class PointsTest(unittest.TestCase):
    def test_without_device_first_click_places_a(self):
        p = MeasurementPoints()
        self.assertEqual(p.click((1.0, 1.0)), "A")
        self.assertEqual(p.click((2.0, 2.0)), "B")
        self.assertIsNone(p.click((3.0, 3.0)))
        self.assertIsNotNone(p.result)

    def test_live_a_follows_device_and_click_places_b(self):
        p = MeasurementPoints()
        self.assertTrue(p.device_moved((1.0, 1.0)))
        self.assertEqual(p.click((2.0, 2.0)), "B")
        p.device_moved((1.5, 1.5))
        self.assertEqual(p["A"], (1.5, 1.5))

    def test_manual_a_is_not_overwritten_until_attach(self):
        p = MeasurementPoints()
        p.click((1.0, 1.0))           # A вручную, позиции устройства ещё нет
        self.assertFalse(p.device_moved((5.0, 5.0)))
        self.assertEqual(p["A"], (1.0, 1.0))
        self.assertTrue(p.attach_a())
        self.assertEqual(p["A"], (5.0, 5.0))

    def test_new_measurement_keeps_live_a(self):
        p = MeasurementPoints()
        p.device_moved((1.0, 1.0))
        p.click((2.0, 2.0))
        p.new_measurement()
        self.assertEqual((p["A"], p["B"]), ((1.0, 1.0), None))

    def test_load_detaches_a(self):
        p = MeasurementPoints()
        p.device_moved((1.0, 1.0))
        p.load((2.0, 2.0), (3.0, 3.0))
        p.device_moved((4.0, 4.0))
        self.assertEqual(p["A"], (2.0, 2.0))


if __name__ == "__main__":
    unittest.main()
