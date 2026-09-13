import unittest
import numpy as np
from coplanar import overlapping_faces


class CoplanarTriangleTests(unittest.TestCase):
    def setUp(self):
        self.triangle = np.array([[0., 0., 0.], [1., 0., 0.], [0., 1., 0.]])

    def test_partial_overlap_without_shared_vertices(self):
        other = self.triangle + [.2, .2, 0]
        self.assertEqual(len(overlapping_faces([self.triangle, other])), 1)

    def test_edge_contact_is_not_overlap(self):
        other = np.array([[1., 0., 0.], [1., 1., 0.], [0., 1., 0.]])
        self.assertEqual(overlapping_faces([self.triangle, other]), [])

    def test_culled_opposing_faces_are_not_a_depth_conflict(self):
        self.assertEqual(overlapping_faces([self.triangle, self.triangle[::-1]]), [])

    def test_recess_separates_visible_surfaces(self):
        self.assertEqual(overlapping_faces([self.triangle, self.triangle+[0, 0, .001]]), [])

    def test_float_export_error_still_detected(self):
        self.assertEqual(len(overlapping_faces([self.triangle, self.triangle+[0, 0, 1e-7]])), 1)

    def test_crossing_non_coplanar_faces_are_not_reported(self):
        other = self.triangle.copy()
        other[1, 2] = .1
        self.assertEqual(overlapping_faces([self.triangle, other]), [])


if __name__ == '__main__':
    unittest.main()
