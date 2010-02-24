README for Voice Age G.729 Codec
================================

Robert Jongbloed, Post Increment 

Contents
========

	1. INTRODUCTION
	2. BUILDING WITH WINDOWS
	3. LICENSING


1. INTRODUCTION
===============

This directory contains the files needed to use the Voice Age G.729 codec with 
OPAL. This codec is only available under Windows, although theoretically is would
be possible to load the appropriate DLLs under Linux using one of the emulation
packages available.


2. BUILDING WITH WINDOWS
========================

1. Obtain the actual codec libraries from http://www.voiceage.com/freecodecs.php

2. Unpack into the "va_g729" directory within this directory

3. Ensure that this directory contains at least the followint:

            va_g729a.h
            va_g729a.lib

4. Add VoiceAgeG729.vcproj to your solution file and compile.


2. BUILDING WITH WINDOWS MOBILE
===============================

1. You will have to purchase the codec from Void Age, this one isn't free!

2. Unpack into the "va_g729_AMR" directory within this directory

3. Ensure that this directory contains at least the followint:

            g729ab_if.h
            typedef.h
            g729ab.lib

4. Add VoiceAgeG729.vcproj to your solution file and compile.


3. LICENSING
============

You must obtain and license the Voice Age codec from that company. 

This code is supplied solely for evaluation and research purposes. The provision of this 
code by Post Increment to any entity does not constitute the supply of a license to that
entity to use, modify or distribute the code in any form, nor does it indemnify the 
entity against any legal actions that may arise from use of this code in any way.

