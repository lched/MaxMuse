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
   100,
   100,
   720,
   430
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
     "text": "MaxMuse.receiver",
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
     "text": "Receives an LSL stream as floats at the stream's own rate, one outlet per channel (output right-to-left). Samples come in Bluetooth-packet bursts; for evenly timed values use MaxMuse.receiver~ into snapshot~.",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      50,
      700,
      35
     ],
     "linecount": 2,
     "id": "obj-2"
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
      110,
      54,
      22
     ],
     "id": "obj-3"
    }
   },
   {
    "box": {
     "maxclass": "message",
     "text": "flush",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "patching_rect": [
      80,
      110,
      47,
      22
     ],
     "id": "obj-4"
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
      130,
      110,
      40,
      22
     ],
     "id": "obj-5"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "MaxMuse.receiver type ACC 3",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "",
      ""
     ],
     "patching_rect": [
      20,
      150,
      220,
      22
     ],
     "id": "obj-6"
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
      190,
      70,
      22
     ],
     "id": "obj-7"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "ACC x",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      215,
      80,
      20
     ],
     "id": "obj-8"
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
      110,
      190,
      70,
      22
     ],
     "id": "obj-9"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "ACC y",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      110,
      215,
      80,
      20
     ],
     "id": "obj-10"
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
      200,
      190,
      70,
      22
     ],
     "id": "obj-11"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "ACC z",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      200,
      215,
      80,
      20
     ],
     "id": "obj-12"
    }
   },
   {
    "box": {
     "maxclass": "newobj",
     "text": "MaxMuse.receiver type EEG 5",
     "numinlets": 1,
     "numoutlets": 5,
     "outlettype": [
      "",
      "",
      "",
      "",
      ""
     ],
     "patching_rect": [
      20,
      260,
      220,
      22
     ],
     "id": "obj-13"
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
      300,
      70,
      22
     ],
     "id": "obj-14"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "TP9",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      325,
      90,
      20
     ],
     "id": "obj-15"
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
      110,
      300,
      70,
      22
     ],
     "id": "obj-16"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "AF7",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      110,
      325,
      90,
      20
     ],
     "id": "obj-17"
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
      200,
      300,
      70,
      22
     ],
     "id": "obj-18"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "AF8",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      200,
      325,
      90,
      20
     ],
     "id": "obj-19"
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
      290,
      300,
      70,
      22
     ],
     "id": "obj-20"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "TP10",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      290,
      325,
      90,
      20
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
      380,
      300,
      70,
      22
     ],
     "id": "obj-22"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "Right AUX",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      380,
      325,
      90,
      20
     ],
     "id": "obj-23"
    }
   },
   {
    "box": {
     "maxclass": "comment",
     "text": "Arguments: property value [channels]  e.g. 'type EEG', 'name Muse-1A2B 5'. Without a channel count the stream must already be running when the object is created.",
     "numinlets": 1,
     "numoutlets": 0,
     "patching_rect": [
      20,
      360,
      700,
      35
     ],
     "linecount": 2,
     "id": "obj-24"
    }
   }
  ],
  "lines": [
   {
    "patchline": {
     "source": [
      "obj-3",
      0
     ],
     "destination": [
      "obj-6",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-4",
      0
     ],
     "destination": [
      "obj-6",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-5",
      0
     ],
     "destination": [
      "obj-6",
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
      "obj-7",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-6",
      1
     ],
     "destination": [
      "obj-9",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-6",
      2
     ],
     "destination": [
      "obj-11",
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
      "obj-14",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-13",
      1
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
      2
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
      "obj-13",
      3
     ],
     "destination": [
      "obj-20",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "obj-13",
      4
     ],
     "destination": [
      "obj-22",
      0
     ]
    }
   }
  ]
 }
}