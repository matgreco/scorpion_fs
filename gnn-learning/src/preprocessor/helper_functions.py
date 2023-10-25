import os
import shutil


def save_to_workspace(input, file_name):
    print("SAVE TO WORKSPACE")
    # Create folder worplace if doesn't exist
    if not os.path.exists("workspace"):
        os.makedirs("workspace")

    file_path = os.path.join("workspace", file_name)
    # Write the input file to the workspace
    with open (file_path, "w") as f:
        f.write(input.read())
        print("ARCHIVO GUARDADO")

def copy_file(source_path, destination_path):
    print("COPY FILE")
    if not os.path.dirname(destination_path) == '':
        if not os.path.exists(os.path.dirname(destination_path)):
            os.makedirs(os.path.dirname(destination_path))

    if os.path.exists(destination_path):
        os.remove(destination_path)

    shutil.copy(source_path, destination_path)
    print("ARCHIVO COPIADO")

def save_priorities(priority_list, destination_path_file):
    with open (destination_path_file, "w") as f:
        f.write("\n".join(priority_list))
        print("ARCHIVO GUARDADO")
