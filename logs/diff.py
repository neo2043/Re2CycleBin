# import os

# f_mt_std =  open("file_mt.log","r+", encoding="utf8")
# f_mt_ntfs = open("file.log","r+", encoding="utf8")

# arr_f_mt_std = f_mt_std.readlines()
# arr_f_mt_ntfs = f_mt_ntfs.readlines()

# print("std len: ", len(arr_f_mt_std))
# print("ntfs len: ", len(arr_f_mt_ntfs))

# # print(arr_f_mt_std[50614])

# # for i in range(len(arr_f_mt_ntfs)):
# #     print(i)

# for i in range(len(arr_f_mt_ntfs)):
#     for j in range(len(arr_f_mt_std)):
#         # print(len(arr_f_mt_std))
#         if (arr_f_mt_ntfs[i] == arr_f_mt_std[j]):
#             print("here")
#             arr_f_mt_std.pop(j)
#             print(len(arr_f_mt_std))
            
# # for i in arr_f_mt_std:
# #     print(i)




# from collections import Counter

# def compare_logs_with_counts(file1, file2):
#     with open(file1, "r", encoding="utf8") as f1, open(file2, "r", encoding="utf8") as f2:
#         logs1 = Counter(line.strip() for line in f1)
#         logs2 = Counter(line.strip() for line in f2)

#     print("=== Lines with count differences ===")
#     all_logs = set(logs1) | set(logs2)

#     for log in sorted(all_logs):
#         if logs1[log] != logs2[log]:
#             print(f"{log}")
#             print(f"  file1: {logs1[log]}")
#             print(f"  file2: {logs2[log]}")

# compare_logs_with_counts("file_mt.log","file.log")



# def compare_logs(file1, file2):
#     with open(file1, "r", encoding="utf8") as f1, open(file2, "r", encoding="utf8") as f2:
#         logs1 = set(line.strip() for line in f1)
#         logs2 = set(line.strip() for line in f2)

#     only_in_file1 = logs1 - logs2
#     only_in_file2 = logs2 - logs1

#     print("=== Lines only in file1 ===")
#     for line in sorted(only_in_file1):
#         print(line)

#     print("\n=== Lines only in file2 ===")
#     for line in sorted(only_in_file2):
#         print(line)


# # Usage
# compare_logs("file_mt.log", "file.log")





# from collections import Counter

# def read_log(path):
#     """
#     Reads a log file safely.
#     Unknown characters are replaced with �.
#     Returns a list of normalized log lines.
#     """
#     with open(path, "r", encoding="utf-8", errors="replace") as f:
#         return [line.rstrip() for line in f]


# def compare_logs(file1, file2):
#     logs1 = Counter(read_log(file1))
#     logs2 = Counter(read_log(file2))

#     only_in_file1 = logs1 - logs2
#     only_in_file2 = logs2 - logs1

#     print("=== Logs only in file1 ===")
#     for line, count in only_in_file1.items():
#         for _ in range(count):
#             print(f"- {line}")

#     print("\n=== Logs only in file2 ===")
#     for line, count in only_in_file2.items():
#         for _ in range(count):
#             print(f"+ {line}")


# if __name__ == "__main__":
#     compare_logs("file_mt.log", "file.log")





from collections import Counter

def read_log(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return [line.rstrip() for line in f]

def compare_logs(file1, file2):
    c1 = Counter(read_log(file1))
    c2 = Counter(read_log(file2))

    all_logs = set(c1) | set(c2)

    print("=== Logs only / extra in file1 ===")
    for log in sorted(all_logs):
        diff = c1[log] - c2[log]
        if diff > 0:
            print(f"- ({diff}x) {log}")

    print("\n=== Logs only / extra in file2 ===")
    for log in sorted(all_logs):
        diff = c2[log] - c1[log]
        if diff > 0:
            print(f"+ ({diff}x) {log}")

if __name__ == "__main__":
    compare_logs("file_mt.log", "file_mt_ntfs_walker.log")
