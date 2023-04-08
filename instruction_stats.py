import argparse
import re

class InstructionStatistics:
    def __init__(self, filename):
        self.filename = filename
        self.instructions = {}
        self.num_instructions = 0

    def count_instructions(self):
        with open(self.filename, 'r') as file:
            lines = file.readlines()[3:-4] # Ignore first 3 and last 4 lines
            for line in lines:
                # Extract the ARM instruction as a single string
                fields = line[19:].strip().split()
                if fields:
                    # Replace "#0x" with "#imm" in the instruction string
                    instruction_str = " ".join(fields)
                    instruction_str = re.sub(r'#0x([\da-fA-F]+)', r'#imm', instruction_str)
                    # Add register arguments to the instruction string
                    instruction = instruction_str.split()[0] + " " + " ".join(instruction_str.split()[1:])
                    # Check if the instruction has two variants based on the last argument
                    self.instructions[instruction] = self.instructions.get(instruction, 0) + 1
                    self.num_instructions += 1

    def print_statistics(self):
        if self.num_instructions == 0:
            print("No ARM instructions found in file")
        else:
            total_instructions = sum(self.instructions.values())
            print("Instruction statistics:\n")
            self.instructions = dict(sorted(self.instructions.items(), key=lambda x: x[1], reverse=True))
            for instruction, count in self.instructions.items():
                percentage = count/total_instructions * 100
                print(f"{instruction}: {count} ({percentage:.2f}%)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Count the frequency of ARM instructions in a binary file")
    parser.add_argument("filename", help="the name of the file to analyze")
    args = parser.parse_args()

    stats = InstructionStatistics(args.filename)
    stats.count_instructions()
    stats.print_statistics()
