#!/usr/bin/env python3
"""
Automatic OpenSCAD STL Generator for Bolt Mount

This program automatically generates an STL file using OpenSCAD for accurate boolean operations.
If OpenSCAD is not installed, it provides clear instructions for manual completion.

Usage:
    python automatic_openscad_generator.py -m 3 -s 20 -d 5 -r 8

Parameters:
    -m, --bolt-size    : Bolt size (M standard), e.g., 3 for M3 (mm)
    -s, --separation   : Separation between bolt holes (mm)
    -d, --depth        : Depth/thickness of the block (mm)
    -r, --radius       : Radius of material around each hole (mm)

Output:
    An STL file named 'bolt_mount_M{m}_S{s}_D{d}_R{r}.stl'
"""

import argparse
import os
import subprocess
import sys
import tempfile

def create_openscad_file(bolt_size, separation, depth, radius):
    """Create OpenSCAD code for a rounded rectangular block with two bolt holes."""
    
    # Calculate bolt hole radius (slightly larger than nominal size for clearance)
    bolt_radius = bolt_size / 2 * 1.1  # Add 10% clearance
    
    # Create the OpenSCAD code
    scad_code = f"""
// Bolt Mount
// M{bolt_size}, Separation: {separation}mm, Depth: {depth}mm, Radius: {radius}mm

// Parameters
bolt_radius = {bolt_radius};
separation = {separation};
depth = {depth};
corner_radius = {radius};

// Main shape - union of a box and two cylinders at the ends
difference() {{
    union() {{
        // Center box
        translate([corner_radius, 0, 0])
            cube([separation, corner_radius*2, depth]);
        
        // Left cylinder
        translate([corner_radius, corner_radius, 0])
            cylinder(h=depth, r=corner_radius, $fn=64);
        
        // Right cylinder
        translate([corner_radius + separation, corner_radius, 0])
            cylinder(h=depth, r=corner_radius, $fn=64);
    }}
    
    // Left bolt hole
    translate([corner_radius, corner_radius, -0.1])
        cylinder(h=depth + 0.2, r=bolt_radius, $fn=32);
    
    // Right bolt hole
    translate([corner_radius + separation, corner_radius, -0.1])
        cylinder(h=depth + 0.2, r=bolt_radius, $fn=32);
}}
"""
    return scad_code

def find_openscad():
    """Find the OpenSCAD executable."""
    # List of possible OpenSCAD executable names and paths
    candidates = [
        "openscad",                            # Linux/macOS
        "OpenSCAD",                            # macOS Application
        r"C:\Program Files\OpenSCAD\openscad.exe",  # Windows default
        r"C:\Program Files (x86)\OpenSCAD\openscad.exe"  # Windows 32-bit on 64-bit
    ]
    
    for candidate in candidates:
        try:
            subprocess.run([candidate, "--version"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
            return candidate
        except (subprocess.SubprocessError, FileNotFoundError):
            continue
    
    return None

def generate_stl_file(bolt_size, separation, depth, radius, output_filename):
    """Generate an STL file using OpenSCAD."""
    
    # Create a temporary file for the OpenSCAD code
    with tempfile.NamedTemporaryFile(suffix='.scad', delete=False) as temp_file:
        temp_filename = temp_file.name
        temp_file.write(create_openscad_file(bolt_size, separation, depth, radius).encode('utf-8'))
    
    try:
        # Find OpenSCAD
        openscad_exe = find_openscad()
        
        if openscad_exe:
            print(f"Found OpenSCAD at: {openscad_exe}")
            print(f"Generating STL file...")
            
            # Run OpenSCAD to generate the STL
            result = subprocess.run(
                [openscad_exe, "-o", output_filename, temp_filename],
                stdout=subprocess.PIPE, 
                stderr=subprocess.PIPE,
                check=False
            )
            
            if result.returncode == 0:
                print(f"Successfully generated STL file: {output_filename}")
                return True
            else:
                print("Error generating STL file:")
                print(result.stderr.decode('utf-8'))
                return False
        else:
            print("OpenSCAD not found. The SCAD file has been saved, but you need to convert it manually.")
            # Save the SCAD file for manual conversion
            scad_filename = output_filename.replace('.stl', '.scad')
            with open(scad_filename, 'w') as f:
                f.write(create_openscad_file(bolt_size, separation, depth, radius))
            print(f"SCAD file saved as: {scad_filename}")
            print("To convert to STL:")
            print(f"1. Install OpenSCAD from https://openscad.org/downloads.html")
            print(f"2. Open the SCAD file in OpenSCAD")
            print(f"3. Use File > Export > Export as STL to save the STL file")
            return False
    finally:
        # Clean up the temporary file
        try:
            os.unlink(temp_filename)
        except:
            pass

def main():
    parser = argparse.ArgumentParser(description='Generate an STL file for a bolt mount')
    parser.add_argument('-m', '--bolt-size', type=float, required=True, help='Bolt size (M standard)')
    parser.add_argument('-s', '--separation', type=float, required=True, help='Separation between bolt holes (mm)')
    parser.add_argument('-d', '--depth', type=float, required=True, help='Depth/thickness of the block (mm)')
    parser.add_argument('-r', '--radius', type=float, required=True, help='Radius of material around each hole (mm)')
    parser.add_argument('-o', '--output', type=str, help='Output filename (default: bolt_mount_M{m}_S{s}_D{d}_R{r}.stl)')
    args = parser.parse_args()
    
    # Generate default filename if not specified
    if not args.output:
        output_filename = f'bolt_mount_M{args.bolt_size}_S{args.separation}_D{args.depth}_R{args.radius}.stl'
    else:
        output_filename = args.output
    
    # Generate the STL file
    success = generate_stl_file(args.bolt_size, args.separation, args.depth, args.radius, output_filename)
    
    if success:
        print(f'Model details:')
        print(f' - Width: {args.separation + 2*args.radius:.2f} mm')
        print(f' - Height: {2*args.radius:.2f} mm')
        print(f' - Depth: {args.depth:.2f} mm')
        print(f' - Bolt size: M{args.bolt_size} (hole diameter: {args.bolt_size * 1.1:.2f} mm)')
        print(f' - Bolt separation: {args.separation:.2f} mm')

if __name__ == '__main__':
    main()

