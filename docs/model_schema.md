# Model Schema

This document defines the JSON contract used by VulcanCAD model import/export.

## Top-level object

Required fields:
- schema_version: currently "1.0"
- name: user-visible model name
- units: default "mm"
- parameters: object map of named parameters
- features: array of feature records
- metadata: object with additional export information

Example:

```json
{
	"schema_version": "1.0",
	"name": "Bracket",
	"units": "mm",
	"parameters": {
		"height": 25.0,
		"depth": "#height"
	},
	"features": [
		{
			"id": 1,
			"type": "PartContainer",
			"name": "Part.001",
			"state": "Valid",
			"suppressed": false,
			"expanded": true,
			"parent_id": null,
			"dependencies": []
		},
		{
			"id": 2,
			"type": "SketchFeature",
			"name": "Sketch.001",
			"state": "Valid",
			"suppressed": false,
			"expanded": true,
			"parent_id": 1,
			"dependencies": [1]
		},
		{
			"id": 3,
			"type": "ExtrudeFeature",
			"name": "Pad.001",
			"state": "Valid",
			"suppressed": false,
			"expanded": true,
			"parent_id": 1,
			"dependencies": [1],
			"profile_id": 2
		}
	],
	"metadata": {
		"generator": "VulcanCAD"
	}
}
```

## Parameters

- Parameter values can be scalars or references.
- Reference syntax uses # prefix.
- Example: "depth": "#height".
- Deserializer resolves references before building features.
- Circular references are rejected.

## Features

Each feature record supports:
- id: unsigned integer ID from file
- type: known feature type string
- name: feature label
- state: Valid, Warning, Error, Suppressed
- suppressed: boolean
- expanded: UI expansion state persisted between runs
- parent_id: nullable parent feature ID
- dependencies: array of feature IDs required before construction

Optional feature payload:
- payload: object with type-specific fields
- profile_id: optional profile dependency (for extrude-like features)

## Deserialization rules

- Unknown schema_version is rejected.
- Missing required fields are reported.
- Unknown feature type does not crash import:
	- feature is still created as fallback type
	- feature state is marked Error
	- issue is added to feature error list
- Broken dependency graph is rejected.

## Serialization modes

- Pretty-print mode: human-editable JSON (2-space indent)
- Compact mode: minified JSON
