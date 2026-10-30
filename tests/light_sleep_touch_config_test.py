"""Exercise the external component's schema through ESPHome's config loader."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


COMPONENTS = Path(__file__).resolve().parents[1] / "components"


class LightSleepTouchConfigTest(unittest.TestCase):
    def check_config(self, sleep_config, *, platform="esp32", error=None):
        hardware = (
            "esp32:\n  variant: esp32s3\n  framework:\n    type: esp-idf\n"
            if platform == "esp32"
            else "host:\n"
        )
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / "sleep.yaml"
            config.write_text(
                "esphome:\n  name: sleep-test\n"
                + hardware
                + f"external_components:\n  - source:\n      type: local\n      path: {COMPONENTS}\n"
                + "    components: [light_sleep_touch]\nlight_sleep_touch:\n"
                + sleep_config
            )
            result = subprocess.run(
                [sys.executable, "-m", "esphome", "config", str(config)],
                capture_output=True,
                text=True,
            )
            output = result.stdout + result.stderr
            if error is None:
                self.assertEqual(result.returncode, 0, output)
            else:
                self.assertNotEqual(result.returncode, 0, output)
                self.assertIn(error, output)

    def test_defaults_and_disabled_inactivity(self):
        self.check_config("  touch_irq_pin: GPIO18\n  timeout: 0s\n")

    def test_zero_sleep_duration(self):
        self.check_config(
            "  touch_irq_pin: GPIO18\n  sleep_duration: 0s\n", error="must be higher than 0s"
        )

    def test_inverted_pin(self):
        self.check_config(
            "  touch_irq_pin:\n    number: GPIO18\n    inverted: true\n",
            error="must not be inverted",
        )

    def test_platform_dependency(self):
        self.check_config("  touch_irq_pin: 18\n", platform="host", error="esp32")


if __name__ == "__main__":
    unittest.main()
