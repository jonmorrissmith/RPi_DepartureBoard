# 3D Print Panel Joiners

A Python tool to create joiners for connecting matrix-panels.

# What is the problem this solves?

Now you have your Departure Board you probably want a way to join the panels together.

While there's the option to get creative and build all manner of cases, I wanted a simple plastic widget which allowed me to use the bolt-holes on the panels to join together adjacent units.

# What you'll need

* Your Raspberry Pi
* Access to a 3D printer

# Creating the STL files

## Some caveats

Neither Python or creating STL files are in my area of expertise, so this might not be the optimal solution.

The problem I faced was creating the holes - there doesn't appear to be an easy way to do this!

The solution was to create an OpenSCAD file and use the command line to convert to STL.

## Installation

Install OpenSCAD - it's quite large so gives you time to make a cup of tea.
```
sudo apt-get install openscad
```
Create a Python virtual environment...
```
python -m venv  ./.stl
```
... and activate it
```
source .stl/bin/activate
```
Now install the SolidPython library
```
pip install solidpython
```
# Creating the Joiners
## Using SolidPython

There are 4 options
* **Bolt Size** - the 'M' size of the bolt-holes on your LED panel
* **Separation** - the distance (in mm) between the centres of the holes
* **Depth** - how thick you want your joiner to be (in mm)
* **Radius** - how wide you want your joiner to be. Radius from the holes (in mm)

Options set using these options:
```
python ./joiner_generator_SolidPython.py --help
usage: joiner_generator.py [-h] -m BOLT_SIZE -s SEPARATION -d DEPTH -r RADIUS

Generate an STL file for a bolt mount

options:
  -h, --help            show this help message and exit
  -m BOLT_SIZE, --bolt-size BOLT_SIZE
                        Bolt size (M standard)
  -s SEPARATION, --separation SEPARATION
                        Separation between bolt holes (mm)
  -d DEPTH, --depth DEPTH
                        Depth/thickness of the block (mm)
  -r RADIUS, --radius RADIUS
                        Radius of material around each hole (mm)
```

For the panels I used (see the main README) I selected the following options.  
```
python ./joiner_generator_SolidPython.py -m 3 -s 16 -d 5 -r 10
```
This is the output you should see
```
OpenSCAD file generated: bolt_mount_M3.0_S16.0_D5.0_R10.0.scad
Converting to STL...
Geometries in cache: 8
Geometry cache size in bytes: 43840
CGAL Polyhedrons in cache: 2
CGAL cache size in bytes: 528288
Total rendering time: 0:00:12.899
   Top level object is a 3D object:
   Simple:        yes
   Vertices:      260
   Halfedges:     780
   Edges:         390
   Halffacets:    264
   Facets:        132
   Volumes:         2
STL file generated: bolt_mount_M3.0_S16.0_D5.0_R10.0.stl
Model details:
 - Width: 36.00 mm
 - Height: 20.00 mm
 - Depth: 5.00 mm
 - Bolt size: M3.0
 - Bolt separation: 16.00 mm
```
This creates both an OpenSCAD file and an STL file.

In true 3D printing style, the filename reflects the options chosen.

## Leaving the Python Virtual Environment ##
Just type
```
deactivate
```
## Using OpenSCAD

This option automatically finds OpenSCAD on your system.

If OpenSCAD isn't found, it saves the SCAD file and provides instructions

Same options - just a different script.
```
python ./joiner_generator_OpenSCAD.py -m 3 -s 16 -d 5 -r 10
```
My preference is the SolidPython method - this way might fix any issues you encounter.

# Print the Joiner

Load the STL file into your favourite slicer and away you go!

I'd definitely recommend printing one first so you can check the fit and adjust accordingly.

I thought I'd measured the separation as 17mm but had to take 1mm off - this may be because of my code!

Once you're happy with the trial, print off another three and you can join the panels together.

I picked up a massive set of bolts from Amazon - 12mm length was just the ticket for a 5mm deep joiner.

Hope this is useful!
