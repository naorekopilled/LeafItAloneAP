from dataclasses import dataclass

from Options import PerGameCommonOptions, Toggle


class RandomStartingLevel(Toggle):
    """Pick one of the three levels as the start and give its unlock. Off starts at Hometown."""
    display_name = "Random Starting Level"


@dataclass
class LeafItAloneOptions(PerGameCommonOptions):
    random_starting_level: RandomStartingLevel
