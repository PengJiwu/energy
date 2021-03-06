from .rapl import RAPL
from .ipmi import IPMI
from .cpufreq import cpuFreq

import subprocess
import threading
import pickle
import time
import os

class monitorProcess:
    def __init__(self, program_name_, sensor_type_='ipmi'):
        self.sensor_type = sensor_type_
        if self.sensor_type == "rapl":
            self.sensor = RAPL("service3")
        elif self.sensor_type == "ipmi":
            self.sensor = IPMI(server= "http://localhost:8080", name_="admin", password_="admin")
        else:
            raise(Exception("Invalid sensor type"))

        self.cpu = cpuFreq()
        self.models = []
        self.header = []
        self.program_name = os.path.basename(program_name_)
        self.program_path = os.path.dirname(program_name_)

    def run(self, list_threads, list_args, idle_time, save_name, verbose=0):
        '''
        TODO list of frequencies
        '''
        current_dir= os.getcwd()
        os.chdir(self.program_path)
        try:
            self.cpu.reset()
            self.cpu.disable_hyperthread()
            self.cpu.set_governors("userspace")
            freq = self.cpu.get_available_frequencies()
            cpus = self.cpu.get_online_cpus()

            for f in freq:
                info_threads = []
                for thr in list_threads:
                    
                    self.cpu.enable_cpu(cpus)
                    self.cpu.set_governors("userspace")
                    self.cpu.disable_cpu(cpus[thr:])
                    self.cpu.set_frequencies(f)
                    #TODO assert len(online_cpus==thr) and freq == f

                    info_pcpu = []
                    for arg in list_args:
                        info_sensor = []

                        arg = list(map(lambda s: str.replace(s, "__nt__", str(thr)), arg))
                        if verbose > 0:
                            print("Argumentos", arg)

                        program = subprocess.Popen(["./"+str(self.program_name)]+arg)
                        ti = time.time()
                        while program.poll() == None:
                            if verbose > 0:
                                print("Tempo", time.time()-ti,
                                    "Frequencia ", self.cpu.get_frequencies(), " nThreads", thr)

                            tg = time.time()
                            info = {"time": time.time()-ti,
                                    "sensor": self.sensor.get_data()}
                            info_sensor.append(info.copy())
                            if verbose > 2:
                                self.sensor.print_data()
                            elif verbose > 1:
                                self.sensor.print_pot()

                            tg = time.time()-tg
    #						if 1-tg >= 0:
    #							time.sleep(1-tg)
                        program.wait()
                        tt = time.time()-ti
                        if verbose > 0:
                            print("Tempo total", tt)

                        l1 = {"arg": list(arg), "total_time": tt,
                            self.sensor_type: info_sensor}
                        info_pcpu.append(l1.copy())

                        time.sleep(idle_time)

                    l2 = {"nthread": thr, "lpcpu": info_pcpu}
                    info_threads.append(l2.copy())

                model = {"freq": f, "threads": info_threads}
                self.models.append(model.copy())

        except Exception as e:
            print("Error", e)

        os.chdir(current_dir)
        if verbose > 0:
            print("Saving")
        self.save(save_name)
        self.cpu.reset()
        # cpu.set_governors("userspace")

    def run_dvfs(self, list_threads, list_args, idle_time, save_name, verbose= 0):
        '''
        TODO list of frequencies
        '''
        current_dir= os.getcwd()
        os.chdir(self.program_path)
        try:
            self.cpu.reset()
            time.sleep(idle_time)
            self.cpu.disable_hyperthread()
            self.cpu.set_governors("ondemand")
            cpus = self.cpu.get_online_cpus()

            info_threads = []
            for thr in list_threads:
                self.cpu.reset()
                time.sleep(idle_time)
                self.cpu.disable_hyperthread()
                self.cpu.set_governors("ondemand")
                self.cpu.disable_cpu(cpus[thr:])
                if verbose > 0:
                    print(cpus[thr:], self.cpu.get_online_cpus())
                #TODO assert len(online_cpus==thr) and freq == f

                info_pcpu = []
                for arg in list_args:
                    arg = list(map(lambda s: str.replace(s, "__nt__", str(thr)), arg))
                    if verbose > 0:
                        print("Argumentos", arg)
                    program = subprocess.Popen(["./"+str(self.program_name)]+arg)
                    info_sensor = []
                    ti = time.time()
                    while program.poll() == None:
                        freqs = self.cpu.get_frequencies()
                        if verbose > 1:
                            print("Tempo", time.time()-ti, "Frequencia ", freqs, " nThreads", thr)
                        tg = time.time()
                        info = {"time": time.time()-ti, "freqs": freqs, "sensor": self.sensor.get_data()}
                        info_sensor.append(info.copy())
                        
                        if verbose > 2:
                            self.sensor.print_data()
                        elif verbose > 1:
                            self.sensor.print_pot()

                        tg = time.time()-tg
                        if 1-tg >= 0:
                            time.sleep(1-tg)

                    program.wait()
                    tt = time.time()-ti
                    if verbose > 0:
                        print("Tempo total", tt)

                    l1 = {"arg": list(arg), "total_time": tt, self.sensor_type: info_sensor}
                    info_pcpu.append(l1.copy())
                    time.sleep(idle_time)

                l2 = {"nthread": thr, "lpcpu": info_pcpu}
                info_threads.append(l2.copy())

        except Exception as e:
            print("Error", e)

        if verbose > 0:
            print("Saving")
        self.models= info_threads
           
        os.chdir(current_dir)
        self.save(save_name)
        
        self.cpu.reset()
        self.cpu.set_governors("ondemand")

    def save(self, save_name):
        '''
        TODO
        '''
        try:
            import platform
            from datetime import datetime
            self.models[0]["header"]= {"kernel":platform.platform(), 
                                        "date":datetime.now().strftime("%I:%M%p on %B %d, %Y")}
            save_path = os.path.dirname(save_name)
            if not os.path.exists(save_path):
                os.makedirs(save_path)
            with open(save_name, "wb") as f:
                pickle.dump(self.models, f, pickle.HIGHEST_PROTOCOL)
        except Exception as e:
            print("Error on saving", e)
