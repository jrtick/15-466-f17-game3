# *Ultimate Blob Tag*

*Ultimate Blob Tag* is *Jordan Tick*'s implementation of [*Ultimate Blob Tag*](http://graphics.cs.cmu.edu/courses/15-466-f17/game3-designs/jrtick) for game3 in 15-466-f17.

![alt text](https://raw.githubusercontent.com/jrtick/15-466-f17-base3/master/screenshots/screenshot.png)

## Asset Pipeline

The assets used for this game are in models/human.blend. They are processed via the blender python api into byte 'blob' files that list out the meshes' vertices, normals, and colors.
Hierarchy is set in player.cpp by parenting the head/arm transforms to the body transform.

## Architecture

The player is moved via WSAD and can extend their arms forward by pressing space. The camera can be rotated with the mouse and zoomed in/out with tab/Lshift. I keep track of the body's rotation so it faces movement direction as well as the rotation of the arms with respect to the body. This way, I can have an accurate collider for all parts of the body and can check if anyone's arm has touched someone's body to make them tagged. This is all taken care of in the Player class.

Score is based on how long you stay untagged for AND how many people you tag. The idea was that surviving for two minutes should be about the same score as if the original tagger were to tag everyone.

## Reflection

The game design for this was pretty straightforward. I had to look up how to pass uniforms into the GL shaders so that shirt colors would change when people were tagged and I had to draw out collision detection for cylinders and spheres to understand how to code it.

The philosophy for the AI is that 1) taggers should chase the closest tagee and 2) tagees should move away from where all the taggers are (opposite the distance-weighted average of their positions). I tried to pretend that the boundaries of the board were also taggers so that tagees would not run out of bounds, but this was unsuccessful for some reason. I must've had a bug somewhere?

# About Base2

This game is based on Base2, starter code for game2 in the 15-466-f17 course. It was developed by Jim McCann, and is released into the public domain.

## Requirements

 - modern C++ compiler
 - glm
 - libSDL2
 - libpng
 - blender (for mesh export script)

On Linux or OSX these requirements should be available from your package manager without too much hassle.

## Building

This code has been set up to be built with [FT jam](https://www.freetype.org/jam/).

### Getting Jam

For more information on Jam, see the [Jam Documentation](https://www.perforce.com/documentation/jam-documentation) page at Perforce, which includes both reference documentation and a getting started guide.

On unixish OSs, Jam is available from your package manager:
```
	brew install ftjam #on OSX
	apt get ftjam #on Debian-ish Linux
```

On Windows, you can get a binary [from sourceforge](https://sourceforge.net/projects/freetype/files/ftjam/2.5.2/ftjam-2.5.2-win32.zip/download),
and put it somewhere in your `%PATH%`.
(Possibly: also set the `JAM_TOOLSET` variable to `VISUALC`.)

### Bulding
Open a terminal (on windows, a Visual Studio Command Prompt), change to this directory, and type:
```
	jam
```

### Building (local libs)

Depending on your OSX, clone 
[kit-libs-linux](https://github.com/ixchow/kit-libs-linux),
[kit-libs-osx](https://github.com/ixchow/kit-libs-osx),
or [kit-libs-win](https://github.com/ixchow/kit-libs-win)
as a subdirectory of the current directory.

The Jamfile sets up library and header search paths such that local libraries will be preferred over system libraries.
