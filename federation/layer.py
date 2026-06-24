class Federation:
    def encode_room(self, memory):
        return hash(str(memory))  # placeholder embedding

    def share(self, room_a, room_b):
        return abs(room_a - room_b) / max(room_a, room_b) if room_a or room_b else 0