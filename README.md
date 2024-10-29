# LoCo - Log combiner

> [!NOTE]
> A LOG - a file recording measurements over time. Maybe related to a nautical term. A piece of wood (a log) on a string is put into the sea and knots in the string are counted to get an estimate of the ships speed. The book the entries from the log were written down into was called the log (-book). 

This is a visualization project.

## What is working right now

- import of many log files, sorting them based on time-stamp
  - boosts rb-trees to sort based on time (Y-m-d H:M:S)
- sequential event detection in an event stream (position and time window)
  - sequential pattern mining algorithm (backend/ from github.com/fidelity/seq2pat/blob/master/sequential/) to extract stories from log streams
- display of detected events as text
  - freetype library to render fonts to png
- example script to generate (random) log files from list of events (exports stories.json)
- visualization of detected events as series of images, render images to a movie
  - uses ImageMagick to composite individual frames with alpha-blending
  - uses ffmpeg as a command line tool to render at an arbitrary framerate into a movie file

![example display of 1,000 detected events](https://github.com/HaukeBartsch/LoCo/blob/main/images/14seconds.gif)

### Discussion

There is a limit to how many things can be displayed over a fixed period of time - perceptual and technical limits. Interestingly other modalities have less issues with such limitations, like sound. You can easily have an object that is moving with more than MACH 1, the speed of sound. And it will still make a noise. What would this correspond to in vision, e.g. in an animation? Moving several times the speed of the perceptual or technological limit? Would we be able to still 'see' something?

Technological limit: That one is easy(-er). Nothing prevents us from creating a slow animation and speeding it up. We can generate 1,000 individual frames and combine them into a single movie. We can display the movie on a theoretical device (without a technical limitation) and capture it again on a real device (with the technical limitation). We would see what a really fast animation would look like.

Perceptual limit: Lets start slow and ramp up. We would like to reach a speed of 1,000 images per second - just because those are round numbers. This is of course trivial if all 1,000 images are the same (**depends on content**). If all the images are different we would have a harder time to see/detect/distinguish them. It could work, if the 1,000 images are all 1 pixel large and arranged in a grid. We would see them as one 32 x 32 image made up out of 1,000 pixel. What if it is instead 2 complex and large images that appear in a repeated sequence - at 1,000 images per second. In that case sorting the images would allow us to 'see' them 'better', the first half second we see the first, the second half second we see the second (**depends on order**).

We could use a sound equivalent of 'compression' - to get a sound from vision. Lets say that our medium to display text provides the barriers. LatticeBoltzmann perhaps? Pixel interactions? Interference? White paper as a substrate. A pixel in black as a solid block, alpha as squishy-ness? 


## Debug / builds

Create the executables using cmake

```bash
cmake -DCMAKE_BUILD_TYPE=Debug .
make
cd renderStories
cmake -DCMAKE_BUILD_TYPE=Debug .
make
```

### Memory leaks

Trying to find some memory leaks on MacOS with


```bash
leaks --atExit -- ./LoCo ....
```

## Usage

We assume that there exist a directory with log files. Each log entry is coded as a line starting with a date/time entry.

```
2019-04-12 12:12:01: INFO This is a log entry.
```

Over time log entries from different files will tell how events follow each other. They tell stories. Understanding such stories will help us understand the system that generates them. We do not want to do error detection or find unusual events. We want to understand the normal operation of a system.

If you do not have an example source of log files you may use a log file generator in the 'testdata' folder.

Once you have the logs that document our events you can analyze them to find sequential pattern using the LoCo executable.

Test if calling the executable on the command line shows it usage help.

```{bash}
LoCo: Log combiner and event detection.

Example:
  LoCo --verbose data/*.log
  >>> 7, 3

A REPL allows you to search for pattern throughout the history. Specify a center log-entry
as proportion 0..1 or index position if greater than 1 and how many history entries
as proportion 0..1 or absolute number or '<number>s' for seconds around the location.

Allowed options:
  -h [ --help ]                        Print this help.
  -s [ --numSplits ] arg               Number of splits used to represent 
                                       single history as sequences [6].
  -l [ --limit ] arg                   Limit the maximum distance allowed 
                                       between log entries [10].
  -m [ --minNumberOfObservations ] arg An event has to occur at least that many
                                       times [3]. Can be set the same as 
                                       numSplits.
  -e [ --maxNumberOfPattern ] arg      Some logs can produce a very large 
                                       number of pattern, stop generating more 
                                       if you reach this limit [1000].
  -c [ --cmd ] arg                     Run this command [.5 300].
  -V [ --version ]                     Print the version number.
  -v [ --verbose ]                     Print more verbose output during 
                                       processing.
  --logfiles arg                       Log files, either folder or list of .log
                                       files. We assume that log entries are 
                                       prefixed with 'Y-m-d H:M:S:'.
```

Starting the program will start a REPL which accepts some special commands:

- 'display': Toggle the animation of the result after processing
- 'save bla.json': Will store the output of the next analysis command as a json encoded file. Can be disabled again with 'save bla.json off'.
- example analysis command is: '.5 400<enter>', i.e., go to the middle of the history and use the 800 events before and after to compute sequential pattern.

Saving sequential pattern produces a JSON encoded file like the following:

```{json}
[
    [
        "Het was een donkeren, en stormachtige nacht. [stories]",
        "Het was een donkeren, en stormachtige nacht. [stories]"
    ],
    [
        "Het was een donkeren, en stormachtige nacht. [stories]",
        "Het was een donkeren, en stormachtige nacht. [stories]",
        "Het was een donkeren, en stormachtige nacht. [stories]"
    ],
    [
        "Het was een donkeren, en stormachtige nacht. [stories]",
        "Het was een donkeren, en stormachtige nacht. [stories]",
        "Het was een donkeren, en stormachtige nacht. [stories]",
        "And they all joined forces to pull the tree out of the swamp. [stories]"
    ],
...
```


### Generating a new story

The testdata/ folder contains a script (createTestData01.py) that can be used to generate (random) stories. Here is one of these that has been rendered into an image using renderStory:

![example story](https://github.com/HaukeBartsch/LoCo/blob/main/images/00000004.png)

We can merge (.sh) these stories by successively fusing each images with its 25 neighbors. The sequence of merged images can be played back at an arbitrary frame rate of 2,200 frames per second (see ffmpeg).

The following steps generate a movie from 2,000 detected stories for playback in 2 seconds. Step 1 creates the a log of events following each other in a random order.

```{bash}
mkdir data2
testdata/createTestData01.py -i 2000 -o data2/events.log
```

Detect 2,000 individual stories from this log using LoCo.

```{bash}
./LoCo --verbose -e 2000 data2/
>>> save stories.json; .5 800
```

Each of the 2,000 stories was rendered into a png image

```{bash}
renderStory --font Roboto-Regular.ttf --font_size 12 -o data stories.json
```

And each of the 2,000 png images was rendered with an overlap of 15 frames (using alpha blending) and combined into a movie using ffmpeg (needs to be installed)

```{bash}
merge.sh | parallel -j 4
ffmpeg -framerate 2200 -pattern_type glob -i '/tmp/*.png' -c:v libx264 -pix_fmt yuv420p movie.mp4
```

[example movie at 1000 frames per second](https://github.com/HaukeBartsch/LoCo/blob/main/images/movie.mp4)

Discussion: As you can see this is 2,000 frames in 2 seconds, and it is underwhelming. Using alpha-blending does not allow for a physical embodiment an interaction of text.

It seems that we are missing a component of interference/interaction. We would like to have a physical effect/embodiment that can realize those. We need a physical embodiment for text. Working with the fonts above the rendering of single characters is done on a regular matrix of image pixel. Is there a way to have the image pixel of different letters to interact with each other? The movie shows that they occupy the same space, that should result in pressure.

# Using interference for visualization

Key-words: physical embodiment of text

The individual characters composed of pixel. Pixel represented as *gelatinous* cubes.

![gelatinous cubes](https://github.com/HaukeBartsch/LoCo/blob/main/images/idea.png)

