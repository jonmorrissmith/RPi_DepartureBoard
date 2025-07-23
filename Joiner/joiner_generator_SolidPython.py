#!/usr/bin/env python3
"""
SolidPython STL Generator for Bolt Mount

This program uses SolidPython to generate an STL file for a rectangular block with rounded corners
and two bolt holes. It creates a proper solid model with accurate boolean operations.

Usage:
    python solidpython_generator.py -m 3 -s 20 -d 5 -r 8

Parameters:
    -m, --bolt-size    : Bolt size (M standard), e.g., 3 for M3 (mm)
    -s, --separation   : Separation between bolt holes (mm)
    -d, --depth        : Depth/thickness of the block (mm)
    -r, --radius       : Radius of material around each hole (mm)

Output:
    An STL file named 'bolt_mount_M{m}_S{s}_D{d}_R{r}.stl'

Requirements:
    pip install solidpython
    Install OpenSCAD from https://openscad.org/downloads.html
"""

import argparse
import os
import sys
try:
    from solid import *
    from solid.utils import *
except ImportError:
    print("Error: SolidPython not installed. Please install it with:")
    print("pip install solidpython")
    sys.exit(1)

def create_bolt_mount(bolt_size, separation, depth, radius):
    """Create a rounded rectangular block with two bolt holes using SolidPython."""
    
    # Calculate bolt hole radius (slightly larger than nominal size for clearance)
    bolt_radius = bolt_size / 2 * 1.1  # Add 10% clearance
    
    # Create the base shape - a union of a box and two cylinders
    center_box = translate([radius, 0, 0])(
        cube([separation, radius*2, depth])
    )
    
    left_cylinder = translate([radius, radius, 0])(
        cylinder(h=depth, r=radius, segments=64)
    )
    
    right_cylinder = translate([radius + separation, radius, 0])(
        cylinder(h=depth, r=radius, segments=64)
    )
    
    base_shape = union()(
        center_box,
        left_cylinder,
        right_cylinder
    )
    
    # Create the bolt holes
    left_hole = translate([radius, radius, -0.1])(
        cylinder(h=depth + 0.2, r=bolt_radius, segments=32)
    )
    
    right_hole = translate([radius + separation, radius, -0.1])(
        cylinder(h=depth + 0.2, r=bolt_radius, segments=32)
    )
    
    # Subtract the holes from the base shape
    bolt_mount = difference()(
        base_shape,
        left_hole,
        right_hole
    )
    
    return bolt_mount

def main():
    parser = argparse.ArgumentParser(description='Generate an STL file for a bolt mount')
    parser.add_argument('-m', '--bolt-size', type=float, required=True, help='Bolt size (M standard)')
    parser.add_argument('-s', '--separation', type=float, required=True, help='Separation between bolt holes (mm)')
    parser.add_argument('-d', '--depth', type=float, required=True, help='Depth/thickness of the block (mm)')
    parser.add_argument('-r', '--radius', type=float, required=True, help='Radius of material around each hole (mm)')
    args = parser.parse_args()
    
    # Create the bolt mount
    bolt_mount = create_bolt_mount(args.bolt_size, args.separation, args.depth, args.radius)
    
    # Generate filenames
    base_name = f'bolt_mount_M{args.bolt_size}_S{args.separation}_D{args.depth}_R{args.radius}'
    scad_filename = f'{base_name}.scad'
    stl_filename = f'{base_name}.stl'
    
    # Save as SCAD file
    scad_render_to_file(bolt_mount, scad_filename, include_orig_code=True)
    print(f'OpenSCAD file generated: {scad_filename}')
    
    # Try to convert to STL automatically
    try:
        print(f'Converting to STL...')
        os.system(f'openscad -o {stl_filename} {scad_filename}')
        
        if os.path.exists(stl_filename):
            print(f'STL file generated: {stl_filename}')
        else:
            print(f'Error: STL file not generated. Is OpenSCAD installed and in your PATH?')
            print(f'You can manually open the SCAD file in OpenSCAD and export to STL.')
    except Exception as e:
        print(f'Error converting to STL: {e}')
        print(f'You can manually open the SCAD file in OpenSCAD and export to STL.')
    
    print(f'Model details:')
    print(f' - Width: {args.separation + 2*args.radius:.2f} mm')
    print(f' - Height: {2*args.radius:.2f} mm')
    print(f' - Depth: {args.depth:.2f} mm')
    print(f' - Bolt size: M{args.bolt_size}')
    print(f' - Bolt separation: {args.separation:.2f} mm')

if __name__ == '__main__':
    main()
