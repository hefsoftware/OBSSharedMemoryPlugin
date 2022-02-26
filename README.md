# OBS Shared Memory Plugin

## Introduction

This plugin allows to have a source in OBS that allows an external process to create dynamic overlays. 
The other process can use SharedMemoryIPC library (https://github.com/hefsoftware/SharedImageIPC) to generate the images. 
Only windows is supported for now.