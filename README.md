# LoCo - Log combiner

> [NOTE!]
> A LOG - a file recording measurements over time. Maybe related to a nautical term. A piece of wood (a log) on a string is put into the sea and knots in the string are counted to get an estimate of the ships speed. The book the entries from the log were written down into was called the log (-book). 

This is a visualization project.

## What is working right now

- import of many log files, sorting them based on a time-stamp
- sequential event detection based on a location and time window in the log stream
- display of detected events as text
- example script to generate (random) log files

![example display of 1,000 detected events](https://github.com/HaukeBartsch/LoCo/blob/main/images/14seconds.gif)

### Discussion

There is a limit to how many things can be displayed over a fixed period of time - perceptual and technical limits. Interestingly other modalities have less issues with such limitations, like sound. You can easily have an object that is moving with more than MACH 1, the speed of sound. And it will still make a noise. What would this correspond to in vision, e.g. in an animation? Moving several times the speed of the perceptual or technological limit? Would we be able to still 'see' something?

Technological limit: That one is easy(-er). Nothing prevents us from creating a slow animation and speeding it up. We can generate 1,000 individual frames and combine them into a single movie. We can display the movie on a theoretical device (without a technical limitation) and capture it again on a real device (with the technical limitation). We would see what a really fast animation would look like.

Perceptual limit: Lets start slow and ramp up. We would like to reach a speed of 1,000 images per second - just because those are round numbers. This is of course trivial if all 1,000 images are the same (**depends on content**). If all the images are different we would have a harder time to see/detect/distinguish them. It could work, if the 1,000 images are all 1 pixel large and arranged in a grid. We would see them as one 32 x 32 image made up out of 1,000 pixel. What if it is instead 2 complex and large images that appear in a repeated sequence - at 1,000 images per second. In that case sorting the images would allow us to 'see' them 'better', the first half second we see the first, the second half second we see the second (**depends on order**).

See render_char_to_28x28 for freetype implementation of text to image.

## Debug builds

```bash
cmake -DCMAKE_BUILD_TYPE=Debug .
```

### Memory leaks

There might be an issue with the memory handling in this module. Trying to find some of the issues with


```bash
leaks --atExit -- ./LoCo ....
```

## Usage

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
  -c [ --cmd ] arg                     Run this command [.5 200].
  -V [ --version ]                     Print the version number.
  -v [ --verbose ]                     Print more verbose output during 
                                       processing.
  --logfiles arg                       Log files, either folder or list of .log
                                       files. We assume that log entries are 
                                       prefixed with 'Y-m-d H:M:S:'.
```
