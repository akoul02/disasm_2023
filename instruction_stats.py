import argparse

class InstructionStatistics:
    def __init__(self, filename):
        self.filename = filename
        self.instructions = {}
        self.num_instructions = 0

    def count_instructions(self):
        with open(self.filename, 'r') as file:
            lines = file.readlines()[3:-4] # Ignore first 3 and last 4 lines
            for line in lines:
                # Extract the ARM instruction as a single string that ends with whitespace
                fields = line[19:].strip().split()
                if fields:
                    instruction = fields[0]
                    if instruction.isalpha():
                        # Check if the instruction has two variants based on the last argument
                        if fields[-1].startswith("#"):
                            instruction += "_i"
                        self.instructions[instruction] = self.instructions.get(instruction, 0) + 1
                        self.num_instructions += 1

    def print_statistics(self):
        if self.num_instructions == 0:
            print("No ARM instructions found in file")
        else:
            total_instructions = sum(self.instructions.values())
            print("Instruction statistics:\n")
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
