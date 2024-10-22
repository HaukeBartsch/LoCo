
## debug builds

```bash
cmake -DCMAKE_BUILD_TYPE=Debug .

```

### Memory leaks

There seems to be an issue with the memory handling in this module. Trying to find some of the issues with


```bash
leaks --atExit -- ./LoCo ....
```

