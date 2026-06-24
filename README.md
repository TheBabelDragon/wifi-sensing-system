# WiFi CSI Spatial Intelligence System v1.1.0

**Early bidirectional integration with aurora-swarm-btc is now active.**

## Important Disclaimer

The command channel from swarm → sensing (`sensing/command_listener.py`) is in an early stage:

- Commands are currently **unauthenticated** and minimally validated.
- This channel should only be used in trusted/internal environments.
- Do not expose it to untrusted networks without adding authentication and validation layers.
- Future versions will include proper security controls.

Use responsibly.