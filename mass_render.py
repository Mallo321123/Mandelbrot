import paramiko
from scp import SCPClient  # type: ignore
from concurrent.futures import ThreadPoolExecutor

def create_ssh_client(hostname, username, key_filename):
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(hostname, username=username, key_filename=key_filename)
    return ssh

def copy_file_to_remote(ssh, local_path, remote_path):
    with SCPClient(ssh.get_transport()) as scp:
        scp.put(local_path, remote_path)

def execute_remote_command(ssh, command):
    stdin, stdout, stderr = ssh.exec_command(command)
    exit_status = stdout.channel.recv_exit_status()
    return stdout.read().decode(), stderr.read().decode(), exit_status

def copy_files_back_from_remote(ssh, remote_folder, local_folder):
    with SCPClient(ssh.get_transport()) as scp:
        scp.get(remote_folder + '/*', local_path=local_folder, recursive=True)

def process_host(host, local_file, remote_folder, local_results_folder, interval, offset):
    print(f"Processing {host['hostname']}...")
    ssh = create_ssh_client(host["hostname"], host["username"], host["key_filename"])

    try:
        # Erstelle den Ordner auf dem Zielrechner
        execute_remote_command(ssh, f"mkdir -p {remote_folder}")

        # Kopiere die Datei auf das Zielsystem
        remote_file = f"{remote_folder}/mandelbrot"
        copy_file_to_remote(ssh, local_file, remote_file)

        # Führe das C++ Programm mit festen und dynamischen Argumenten aus
        command = (
            f"chmod +x {remote_file} && {remote_file} -w 8000 -h 6000 --chunk_size 100 "
            f"--num_workers --interval {interval} --offset {offset}"
        )
        stdout, stderr, status = execute_remote_command(ssh, command)

        if status == 0:
            print(f"Command succeeded on {host['hostname']}: {stdout.strip()}")
        else:
            print(f"Command failed on {host['hostname']}: {stderr.strip()}")

        # Kopiere Dateien aus dem Zielordner zurück ins lokale Verzeichnis
        copy_files_back_from_remote(ssh, remote_folder, local_results_folder)

    finally:
        ssh.close()

def main():
    target_hosts = [
        {"hostname": "192.168.1.101", "username": "root", "key_filename": "/home/mario/.ssh/id_ed25519"},
        {"hostname": "192.168.1.102", "username": "root", "key_filename": "/home/mario/.ssh/id_ed25519"}
    ]
    local_file = "./build/mandelbrot"  # C++ Programm, das kopiert werden soll
    remote_folder = "/home/gast/mandelbrot"  # Zielordner auf den Zielsystemen
    local_results_folder = "./results"  # Ordner zum Speichern der Ergebnisse

    interval = len(target_hosts)

    # Starte parallele Verarbeitung der Hosts
    with ThreadPoolExecutor(max_workers=len(target_hosts)) as executor:
        futures = []
        for offset, host in enumerate(target_hosts):
            futures.append(executor.submit(
                process_host,
                host,
                local_file,
                remote_folder,
                local_results_folder,
                interval,
                offset
            ))

        # Warten auf Abschluss aller Threads
        for future in futures:
            future.result()

if __name__ == "__main__":
    main()