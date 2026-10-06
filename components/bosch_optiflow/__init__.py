"""Bosch Optiflow / PowerBus-over-BLE helpers.

No configuration: loading this component just makes bosch_optiflow.h (frame
encoding, decoding and checksum helpers) available to the lambdas in
packages/bosch-optiflow.yaml.
"""
import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = ["@scottbambrick"]

CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    cg.add_global(
        cg.RawStatement('#include "esphome/components/bosch_optiflow/bosch_optiflow.h"')
    )
