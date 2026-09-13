"""Detect positive-area overlaps of same-facing coplanar exported triangles.

Shared edges and opposing internal faces are not depth conflicts with backface
culling. Testing the exported triangles catches overlaps across material meshes.
"""
from collections import defaultdict
import numpy as np


def cross2(a, b):
    return a[0]*b[1] - a[1]*b[0]


def intersection_polygon(a, b):
    polygon = list(a)
    if cross2(b[1]-b[0], b[2]-b[0]) < 0:
        b = b[::-1]
    for start, end in zip(b, np.roll(b, -1, axis=0)):
        edge = end-start
        result = []
        if not polygon:
            break
        previous = polygon[-1]
        previous_side = cross2(edge, previous-start)
        for point in polygon:
            side = cross2(edge, point-start)
            if (side >= 0) != (previous_side >= 0):
                result.append(previous + (point-previous)*(previous_side/(previous_side-side)))
            if side >= 0:
                result.append(point)
            previous, previous_side = point, side
        polygon = result
    return np.asarray(polygon)


def overlapping_faces(triangles):
    triangles = np.asarray(triangles, dtype=np.float64)
    cross = np.cross(triangles[:,1]-triangles[:,0], triangles[:,2]-triangles[:,0])
    lengths = np.linalg.norm(cross, axis=1)
    normals = cross / np.maximum(lengths[:,None], 1e-30)
    distances = np.einsum('ij,ij->i', normals, triangles[:,0])
    groups = defaultdict(list)
    for i in np.flatnonzero(lengths > 1e-12):
        # Distance is checked continuously below, not binned, to avoid missing
        # near-coplanar surfaces on opposite sides of a quantization boundary.
        groups[tuple(np.round(normals[i], 4))].append(i)
    overlaps = []
    for indices in groups.values():
        if len(indices) < 2:
            continue
        indices = np.asarray(indices)
        dropped = np.argmax(abs(normals[indices[0]]))
        axes = [axis for axis in range(3) if axis != dropped]
        projected = triangles[indices][:,:,axes]
        low, high = projected.min(axis=1), projected.max(axis=1)
        for local, i in enumerate(indices[:-1]):
            candidates = np.flatnonzero(
                (np.arange(len(indices)) > local)
                & (abs(distances[indices]-distances[i]) < 2e-6)
                & np.all(np.minimum(high,high[local])-np.maximum(low,low[local]) > 1e-8, axis=1))
            for other in candidates:
                j = indices[other]
                if np.max(abs((triangles[j]-triangles[i,0]) @ normals[i])) > 2e-6:
                    continue
                polygon = intersection_polygon(projected[local], projected[other])
                if len(polygon) < 3:
                    continue
                area = abs(sum(cross2(a,b) for a,b in zip(polygon,np.roll(polygon,-1,axis=0))))*.5
                if area > 1e-10:
                    overlaps.append((int(i), int(j), float(area)))
    return overlaps
