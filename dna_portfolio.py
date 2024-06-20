import csv
import sys


def main():
    # check if the correct number of command-line arguments is provided
    if len(sys.argv) != 3:
        print("Usage: python dna.py data.csv sequence.txt")
        return 1

    # extract database file and sequences file from command-line arguments
    database_filename = sys.argv[1]
    sequences_filename = sys.argv[2]

    # initialize a list to store database entries
    database_entries = []

    # read database file (CSV) and convert its contents into a list of dictionaries
    with open(database_filename, mode='r') as db_file:
        database_reader = csv.DictReader(db_file)
        database_entries = list(database_reader)
        header_fields = database_reader.fieldnames

    # read DNA sequence file (from simple text) and store its contents as a string
    with open(sequences_filename) as seq_file:
        dna_sequence = seq_file.read()

    # find the longest match of each STR pattern in the DNA sequence
    str_counts = {}
    for header_field in header_fields[1:]:
        str_counts[header_field] = longest_match(dna_sequence, header_field)

    # check the database for profiles with matching STR patterns
    for person_entry in database_entries:
        match_count = 0
        for header_field in header_fields[1:]:
            if int(str_counts[header_field]) == int(person_entry[header_field]):
                match_count += 1
        if match_count == len(header_fields) - 1:
            matched_name = person_entry["name"]
            print(matched_name)
            return 0

    # else
    print("No match")

    return 0


def longest_match(sequence, subsequence):
    """Returns length of longest run of subsequence in sequence."""

    # Initialize variables
    longest_run = 0
    subsequence_length = len(subsequence)
    sequence_length = len(sequence)

    # Check each character in sequence for most consecutive runs of subsequence
    for i in range(sequence_length):

        # Initialize count of consecutive runs
        count = 0

        # Check for a subsequence match in a "substring" (a subset of characters) within sequence
        # If a match, move substring to next potential match in sequence
        # Continue moving substring and checking for matches until out of consecutive matches
        while True:

            # Adjust substring start and end
            start = i + count * subsequence_length
            end = start + subsequence_length

            # If there is a match in the substring
            if sequence[start:end] == subsequence:
                count += 1

            # If there is no match in the substring
            else:
                break

        # Update most consecutive matches found
        longest_run = max(longest_run, count)

    # After checking for runs at each character in seqeuence, return longest run found
    return longest_run


main()
