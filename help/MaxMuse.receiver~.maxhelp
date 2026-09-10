{
 "patcher": {
  "fileversion": 1,
  "appversion": {
   "major": 8,
   "minor": 6,
   "revision": 0,
   "architecture": "x64",
   "modernui": 1
  },
  "classnamespace": "box",
  "rect": [
   80,
   80,
   800,
   820
  ],
  "bglocked": 0,
  "openinpresentation": 0,
  "default_fontsize": 12.0,
  "default_fontface": 0,
  "default_fontname": "Arial",
  "gridonopen": 1,
  "gridsize": [
   15.0,
   15.0
  ],
  "gridsnaponopen": 1,
  "objectsnaponopen": 1,
  "statusbarvisible": 2,
  "toolbarvisible": 1,
  "boxanimatetime": 200,
  "enablehscroll": 1,
  "enablevscroll": 1,
  "description": "",
  "digest": "",
  "tags": "",
  "style": "",
  "subpatcher_template": "",
  "boxes": [
   {
    "box": {
     "maxclass": "comment",
     "text": "MaxMuse.receiver~",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      15,
      700,
      30
     ],
     "fontsize": 22,
     "id": "obj-1"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "Receives an LSL stream (Muse 2 EEG, PPG, ACC, GYRO or any numeric LSL stream) as audio-rate signals, one outlet per channel. Resampled in real time with band-limited interpolation, clock-drift compensation and click-free handling of Bluetooth dropouts.",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      50,
      760,
      50
     ],
     "linecount": 3,
     "id": "obj-2"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "1. Start the headband stream, e.g. in a terminal:  muselsl stream --ppg --acc --gyro     2. Turn audio on.     Arguments: property value [channels]. With a channel count the object is created even before the stream exists.",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      100,
      760,
      35
     ],
     "linecount": 2,
     "id": "obj-3"
    }
   },
   {
    "box": {
     "maxclass": "ezdac~",
     "numinlets": 2,
     "numoutlets": 0,
     "patching_rect": [
      20,
      150,
      45,
      45
     ],
     "id": "obj-4"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "audio on/off (needed)",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      70,
      162,
      200,
      20
     ],
     "id": "obj-5"
    }
   },
   {
    "box": {
     "maxclass": "message",
     "text": "status",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      20,
      215,
      54,
      22
     ],
     "id": "obj-6"
    }
   },
   {
    "box": {
     "maxclass": "message",
     "text": "reset",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      80,
      215,
      47,
      22
     ],
     "id": "obj-7"
    }
   },
   {
    "box": {
     "maxclass": "message",
     "text": "scan",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      135,
      215,
      40,
      22
     ],
     "id": "obj-8"
    }
   },
   {
    "box": {
     "maxclass": "message",
     "text": "latency 0",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      185,
      215,
      75,
      22
     ],
     "id": "obj-9"
    }
   },
   {
    "box": {
     "maxclass": "message",
     "text": "latency 150",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      260,
      215,
      89,
      22
     ],
     "id": "obj-10"
    }
   },
   {
    "box": {
     "maxclass": "message",
     "text": "interp sinc",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      345,
      215,
      89,
      22
     ],
     "id": "obj-11"
    }
   },
   {
    "box": {
     "maxclass": "message",
     "text": "interp cubic",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      430,
      215,
      96,
      22
     ],
     "id": "obj-12"
    }
   },
   {
    "box": {
     "maxclass": "message",
     "text": "interp linear",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      520,
      215,
      103,
      22
     ],
     "id": "obj-13"
    }
   },
   {
    "box": {
     "maxclass": "message",
     "text": "fillgaps $1",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      610,
      215,
      89,
      22
     ],
     "id": "obj-14"
    }
   },
   {
    "box": {
     "maxclass": "toggle",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      "int"
     ],
     "parameter_enable": 0,
     "patching_rect": [
      610,
      190,
      20,
      20
     ],
     "id": "obj-15"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "MaxMuse.receiver~ type EEG 5 @latency 0 @interp sinc",
     "numinlets": 1,
     "numoutlets": 5,
     "outlettype": [
      "signal",
      "signal",
      "signal",
      "signal",
      "signal"
     ],
     "patching_rect": [
      20,
      270,
      640,
      22
     ],
     "id": "obj-16"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "messages: status (console report), reset (resync to live), scan (list LSL streams in the console)",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      245,
      600,
      35
     ],
     "linecount": 2,
     "id": "obj-17"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "snapshot~ 50",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      20,
      310,
      85,
      22
     ],
     "id": "obj-18"
    }
   },
   {
    "box": {
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "bang"
     ],
     "parameter_enable": 0,
     "patching_rect": [
      20,
      345,
      70,
      22
     ],
     "id": "obj-19"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "TP9 (\u00b5V)",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      372,
      120,
      20
     ],
     "id": "obj-20"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "snapshot~ 50",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      150,
      310,
      85,
      22
     ],
     "id": "obj-21"
    }
   },
   {
    "box": {
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "bang"
     ],
     "parameter_enable": 0,
     "patching_rect": [
      150,
      345,
      70,
      22
     ],
     "id": "obj-22"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "AF7 (\u00b5V)",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      150,
      372,
      120,
      20
     ],
     "id": "obj-23"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "snapshot~ 50",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      280,
      310,
      85,
      22
     ],
     "id": "obj-24"
    }
   },
   {
    "box": {
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "bang"
     ],
     "parameter_enable": 0,
     "patching_rect": [
      280,
      345,
      70,
      22
     ],
     "id": "obj-25"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "AF8 (\u00b5V)",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      280,
      372,
      120,
      20
     ],
     "id": "obj-26"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "snapshot~ 50",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      410,
      310,
      85,
      22
     ],
     "id": "obj-27"
    }
   },
   {
    "box": {
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "bang"
     ],
     "parameter_enable": 0,
     "patching_rect": [
      410,
      345,
      70,
      22
     ],
     "id": "obj-28"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "TP10 (\u00b5V)",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      410,
      372,
      120,
      20
     ],
     "id": "obj-29"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "snapshot~ 50",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      540,
      310,
      85,
      22
     ],
     "id": "obj-30"
    }
   },
   {
    "box": {
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "bang"
     ],
     "parameter_enable": 0,
     "patching_rect": [
      540,
      345,
      70,
      22
     ],
     "id": "obj-31"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "Right AUX (\u00b5V)",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      540,
      372,
      120,
      20
     ],
     "id": "obj-32"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "Other Muse 2 sensors are separate LSL streams: one object per stream. Stream types are those advertised by your streamer (muselsl: EEG, PPG, ACC, GYRO). Send 'scan' to list what is on the network.",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      405,
      760,
      35
     ],
     "linecount": 2,
     "id": "obj-33"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "MaxMuse.receiver~ type PPG 3",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "signal",
      "signal",
      "signal"
     ],
     "patching_rect": [
      20,
      455,
      230,
      22
     ],
     "id": "obj-34"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "PPG 64 Hz",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      260,
      455,
      250,
      20
     ],
     "id": "obj-35"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "snapshot~ 50",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      20,
      487,
      85,
      22
     ],
     "id": "obj-36"
    }
   },
   {
    "box": {
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "bang"
     ],
     "parameter_enable": 0,
     "patching_rect": [
      20,
      515,
      70,
      22
     ],
     "id": "obj-37"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "snapshot~ 50",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      130,
      487,
      85,
      22
     ],
     "id": "obj-38"
    }
   },
   {
    "box": {
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "bang"
     ],
     "parameter_enable": 0,
     "patching_rect": [
      130,
      515,
      70,
      22
     ],
     "id": "obj-39"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "snapshot~ 50",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      240,
      487,
      85,
      22
     ],
     "id": "obj-40"
    }
   },
   {
    "box": {
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "bang"
     ],
     "parameter_enable": 0,
     "patching_rect": [
      240,
      515,
      70,
      22
     ],
     "id": "obj-41"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "MaxMuse.receiver~ type ACC 3",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "signal",
      "signal",
      "signal"
     ],
     "patching_rect": [
      20,
      550,
      230,
      22
     ],
     "id": "obj-42"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "accelerometer 52 Hz (x y z)",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      260,
      550,
      250,
      20
     ],
     "id": "obj-43"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "snapshot~ 50",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      20,
      582,
      85,
      22
     ],
     "id": "obj-44"
    }
   },
   {
    "box": {
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "bang"
     ],
     "parameter_enable": 0,
     "patching_rect": [
      20,
      610,
      70,
      22
     ],
     "id": "obj-45"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "snapshot~ 50",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      130,
      582,
      85,
      22
     ],
     "id": "obj-46"
    }
   },
   {
    "box": {
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "bang"
     ],
     "parameter_enable": 0,
     "patching_rect": [
      130,
      610,
      70,
      22
     ],
     "id": "obj-47"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "snapshot~ 50",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      240,
      582,
      85,
      22
     ],
     "id": "obj-48"
    }
   },
   {
    "box": {
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "bang"
     ],
     "parameter_enable": 0,
     "patching_rect": [
      240,
      610,
      70,
      22
     ],
     "id": "obj-49"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "MaxMuse.receiver~ type GYRO 3",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "signal",
      "signal",
      "signal"
     ],
     "patching_rect": [
      20,
      645,
      230,
      22
     ],
     "id": "obj-50"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "gyroscope 52 Hz (x y z)",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      260,
      645,
      250,
      20
     ],
     "id": "obj-51"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "snapshot~ 50",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      20,
      677,
      85,
      22
     ],
     "id": "obj-52"
    }
   },
   {
    "box": {
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "bang"
     ],
     "parameter_enable": 0,
     "patching_rect": [
      20,
      705,
      70,
      22
     ],
     "id": "obj-53"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "snapshot~ 50",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      130,
      677,
      85,
      22
     ],
     "id": "obj-54"
    }
   },
   {
    "box": {
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "bang"
     ],
     "parameter_enable": 0,
     "patching_rect": [
      130,
      705,
      70,
      22
     ],
     "id": "obj-55"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "snapshot~ 50",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      240,
      677,
      85,
      22
     ],
     "id": "obj-56"
    }
   },
   {
    "box": {
     "maxclass": "flonum",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      "bang"
     ],
     "parameter_enable": 0,
     "patching_rect": [
      240,
      705,
      70,
      22
     ],
     "id": "obj-57"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "Total latency = jitter buffer (@latency, auto by default: ~80 ms for Muse EEG) + interpolation look-ahead (sinc 8 samples = 31 ms at 256 Hz, cubic 2, linear 1). Use @interp cubic for slow streams (ACC/GYRO) if latency matters more than image rejection.",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      745,
      760,
      50
     ],
     "linecount": 3,
     "id": "obj-58"
    }
   }
  ],
  "lines": [
   {
    "patchline": {
     "source": [
      "obj-15",
      0
     ],
     "destination": [
      "obj-14",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-6",
      0
     ],
     "destination": [
      "obj-16",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-7",
      0
     ],
     "destination": [
      "obj-16",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-8",
      0
     ],
     "destination": [
      "obj-16",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-9",
      0
     ],
     "destination": [
      "obj-16",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-10",
      0
     ],
     "destination": [
      "obj-16",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-11",
      0
     ],
     "destination": [
      "obj-16",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-12",
      0
     ],
     "destination": [
      "obj-16",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-13",
      0
     ],
     "destination": [
      "obj-16",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-14",
      0
     ],
     "destination": [
      "obj-16",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-16",
      0
     ],
     "destination": [
      "obj-18",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-18",
      0
     ],
     "destination": [
      "obj-19",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-16",
      1
     ],
     "destination": [
      "obj-21",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-21",
      0
     ],
     "destination": [
      "obj-22",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-16",
      2
     ],
     "destination": [
      "obj-24",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-24",
      0
     ],
     "destination": [
      "obj-25",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-16",
      3
     ],
     "destination": [
      "obj-27",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-27",
      0
     ],
     "destination": [
      "obj-28",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-16",
      4
     ],
     "destination": [
      "obj-30",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-30",
      0
     ],
     "destination": [
      "obj-31",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-34",
      0
     ],
     "destination": [
      "obj-36",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-36",
      0
     ],
     "destination": [
      "obj-37",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-34",
      1
     ],
     "destination": [
      "obj-38",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-38",
      0
     ],
     "destination": [
      "obj-39",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-34",
      2
     ],
     "destination": [
      "obj-40",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-40",
      0
     ],
     "destination": [
      "obj-41",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-42",
      0
     ],
     "destination": [
      "obj-44",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-44",
      0
     ],
     "destination": [
      "obj-45",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-42",
      1
     ],
     "destination": [
      "obj-46",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-46",
      0
     ],
     "destination": [
      "obj-47",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-42",
      2
     ],
     "destination": [
      "obj-48",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-48",
      0
     ],
     "destination": [
      "obj-49",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-50",
      0
     ],
     "destination": [
      "obj-52",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-52",
      0
     ],
     "destination": [
      "obj-53",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-50",
      1
     ],
     "destination": [
      "obj-54",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-54",
      0
     ],
     "destination": [
      "obj-55",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-50",
      2
     ],
     "destination": [
      "obj-56",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-56",
      0
     ],
     "destination": [
      "obj-57",
      0
     ]
    }
   }
  ]
 }
}