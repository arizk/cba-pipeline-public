import numpy as np
import matplotlib.pyplot as plt
import pickle
import pandas as pd
from figure_configuration_ieee_standard import figure_configuration_ieee_standard
from scipy.interpolate import interp1d


def main():
    #sbu_client1 = np.load('sbu-100-doubles-tc-mem_client1.npy')
    #sbu_client2 = np.load('sbu-100-doubles-tc-mem_client2.npy')
    #sbuose_client1 = np.load('sbuose-100-doubles-tc-mem_client1.npy')
    #sbuose_client2 = np.load('sbuose-100-doubles-tc-mem_client2.npy')
    #data_pandas = load_results('data_pandas_df.pkl')
    #data_pandas2= load_results('data_pandas_df_2.pkl')
    #data_pandas3 = load_results('data_pandas_df_3.pkl')
    ##trace = np.loadtxt(open("trevor_trace.csv", "rb"), delimiter=",", skiprows=1)
    #
    ## Quality plots singles
    #pd = data_pandas[0].values
    #
    #sbuose_quality = pd[197:99:-1, 3]
    #sbuose_rebuf = pd[197:99:-1, 8]
    #sbuose_extime = pd[197:99:-1, 11]
    #sbuose_qoe = pd[197:99:-1, 9]
    #t_sbuose = pd[197:99:-1, 15]
    #
    #sbu_quality = pd[496:398:-1, 3]
    #sbu_rebuf = pd[496:398:-1, 8]
    #sbu_extime = pd[496:398:-1, 11]
    #sbu_qoe = pd[496:398:-1, 9]
    #t_sbu = pd[496:398:-1, 15]
    #
    # %% Fairness
    #N_points=1000
    #t_fairness = np.linspace(5, 175, num=N_points)
    #sbuose_qoe_client2 = pd[197:99:-1, 9]
    #t_sbuose_client2 = pd[197:99:-1, 15]
    #f_sbuose_qoe_client2 = interp1d(t_sbuose_client2, sbuose_qoe_client2)
    #sbuose_qoe_client2_resam = f_sbuose_qoe_client2(t_fairness)
    #
    #sbuose_qoe_client1 = pd[297:199:-1, 9]
    #t_sbuose_client1 = pd[297:199:-1, 15]
    #f_sbuose_qoe_client1 = interp1d(t_sbuose_client1, sbuose_qoe_client1)
    #sbuose_qoe_client1_resam = f_sbuose_qoe_client1(t_fairness)
    #
    #sbu_qoe_client2 = pd[496:398:-1, 9]
    #t_sbu_client2 = pd[496:398:-1, 15]
    #f_sbu_qoe_client2 = interp1d(t_sbu_client2, sbu_qoe_client2)
    #sbu_qoe_client2_resam = f_sbu_qoe_client2(t_fairness)
    #
    #sbu_qoe_client1 = pd[595:497:-1, 9]
    #t_sbu_client1 = pd[595:497:-1, 15]
    #f_sbu_qoe_client1 = interp1d(t_sbu_client1, sbu_qoe_client1)
    #sbu_qoe_client1_resam = f_sbu_qoe_client1(t_fairness)
    
    
    # %% Fairness
    N_points=1000
    t_fairness = np.linspace(5, 175, num=N_points)
    suffix = '_60-20-20_mem'
    
    # Remove negative values by adding to everything
    #sbu=np.load('sbu_client1{}.npy'.format(suffix))
    #sbu2 = np.load('sbu_client2{}.npy'.format(suffix))
    #sbuose=np.load('sbuose_client1{}.npy'.format(suffix))
    #sbuose2 = np.load('sbuose_client2{}.npy'.format(suffix))
    #panda=np.load('p_client1{}.npy'.format(suffix))
    #panda2 = np.load('p_client2{}.npy'.format(suffix))
    #bola=np.load('bola_client1{}.npy'.format(suffix))
    #bola2 = np.load('bola_client2{}.npy'.format(suffix))
    #linucb=np.load('linucb_client1{}.npy'.format(suffix))
    #linucb2 = np.load('linucb_client2{}.npy'.format(suffix))
    #min_val = np.min([np.min(sbu[:,1]),np.min(sbu2[:,1]),
    #                  np.min(sbuose[:,1]),np.min(sbuose2[:,1]),
    #                  np.min(panda[:,1]),np.min(panda2[:,1]),
    #                  np.min(bola[:,1]),np.min(bola2[:,1]),
    #                  np.min(linucb[:,1]),np.min(linucb2[:,1])])
    #print(min_val)
    #if min_val < 0:
    #    sbu[:,1] = sbu[:,1] + min_val
    #    sbu2[:,1] = sbu2[:,1] + min_val
    #    sbuose[:,1] = sbuose[:,1] + min_val
    #    sbuose2[:,1] = sbuose2[:,1] + min_val
    #    panda[:,1] = panda[:,1] + min_val
    #    panda2[:,1] = panda2[:,1] + min_val
    #    bola[:,1] = bola[:,1] + min_val
    #    bola2[:,1] = bola2[:,1] + min_val
    #    linucb[:,1] = linucb[:,1] + min_val
    #    linucb2[:,1] = linucb2[:,1] + min_val
    
    
    # SBU
    sbu=np.load('sbu_client1{}.npy'.format(suffix))
    sbu2 = np.load('sbu_client2{}.npy'.format(suffix))
    qoenorm=np.min([np.min(sbu[:,1]),np.min(sbu2[:,1])])
    sbu_client1 = sbu[:,1]-qoenorm
    t_sbu_client1 = sbu[:,0]
    f_sbu_qoe_client1 = interp1d(t_sbu_client1, sbu_client1)
    sbu_qoe_client1_resam = f_sbu_qoe_client1(t_fairness)
    
    sbu_client2 = sbu2[:,1]-qoenorm
    t_sbu_client2 = sbu2[:,0]
    f_sbu_qoe_client2 = interp1d(t_sbu_client2, sbu_client2)
    sbu_qoe_client2_resam = f_sbu_qoe_client2(t_fairness)
    
    
    # SBU-OSE
    sbuose=np.load('sbuose_client1{}.npy'.format(suffix))
    sbuose2 = np.load('sbuose_client2{}.npy'.format(suffix))
    qoenorm=np.min([np.min(sbuose[:,1]),np.min(sbuose2[:,1])])
    sbuose_client1 = sbuose[:,1]-qoenorm
    t_sbuose_client1 = sbuose[:,0]
    f_sbuose_qoe_client1 = interp1d(t_sbuose_client1, sbuose_client1)
    sbuose_qoe_client1_resam = f_sbuose_qoe_client1(t_fairness)

    sbuose_client2 = sbuose2[:,1]-qoenorm
    t_sbuose_client2 = sbuose2[:,0]
    f_sbuose_qoe_client2 = interp1d(t_sbuose_client2, sbuose_client2)
    sbuose_qoe_client2_resam = f_sbuose_qoe_client2(t_fairness)


    # PANDA
    panda=np.load('p_client1{}.npy'.format(suffix))
    panda2 = np.load('p_client2{}.npy'.format(suffix))
    qoenorm=np.min([np.min(panda[:,1]),np.min(panda2[:,1])])
    panda_client1 = panda[:,1]-qoenorm
    t_panda_client1 = panda[:,0]
    f_panda_qoe_client1 = interp1d(t_panda_client1, panda_client1)
    panda_qoe_client1_resam = f_panda_qoe_client1(t_fairness)


    panda_client2 = panda2[:,1]-qoenorm
    t_panda_client2 = panda2[:,0]
    f_panda_qoe_client2 = interp1d(t_panda_client2, panda_client2)
    panda_qoe_client2_resam = f_panda_qoe_client2(t_fairness)


    # BOLA
    bola=np.load('bola_client1{}.npy'.format(suffix))
    bola2 = np.load('bola_client2{}.npy'.format(suffix))
    qoenorm=np.min([np.min(bola[:,1]),np.min(bola2[:,1])])
    bola_client1 = bola[:,1]-qoenorm
    t_bola_client1 = bola[:,0]
    f_bola_qoe_client1 = interp1d(t_bola_client1, bola_client1)
    bola_qoe_client1_resam = f_bola_qoe_client1(t_fairness)

    bola_client2 = bola2[:,1]-qoenorm
    t_bola_client2 = bola2[:,0]
    f_bola_qoe_client2 = interp1d(t_bola_client2, bola_client2)
    bola_qoe_client2_resam = f_bola_qoe_client2(t_fairness)

    
    # LinUCB
    linucb=np.load('linucb_client1{}.npy'.format(suffix))
    linucb2 = np.load('linucb_client2{}.npy'.format(suffix))
    qoenorm=np.min([np.min(linucb[:,1]),np.min(linucb2[:,1])])
    linucb_client1 = linucb[:,1]-qoenorm
    t_linucb_client1 = linucb[:,0]
    f_linucb_qoe_client1 = interp1d(t_linucb_client1, linucb_client1)
    linucb_qoe_client1_resam = f_linucb_qoe_client1(t_fairness)

    linucb_client2 = linucb2[:,1]-qoenorm
    t_linucb_client2 = linucb2[:,0]
    f_linucb_qoe_client2 = interp1d(t_linucb_client2, linucb_client2)
    linucb_qoe_client2_resam = f_linucb_qoe_client2(t_fairness)

    # bola = np.load('linucb-100-doubles-tc-membola-100-doubles-tc-na_client1.npy')
    # bola_client1 = bola[:, 1]
    # t_bola_client1 = bola[:, 0]
    # f_bola_qoe_client1 = interp1d(t_bola_client1, bola_client1)
    # bola_qoe_client1_resam = f_bola_qoe_client1(t_fairness)



    sbuose_qoe_fairness = fairness(sbuose_qoe_client1_resam, sbuose_qoe_client2_resam)
    sbu_qoe_fairness = fairness(sbu_qoe_client1_resam, sbu_qoe_client2_resam)
    panda_qoe_fairness = fairness(panda_qoe_client1_resam, panda_qoe_client2_resam)
    bola_qoe_fairness = fairness(bola_qoe_client1_resam, bola_qoe_client2_resam)
    linucb_qoe_fairness = fairness(linucb_qoe_client1_resam, linucb_qoe_client2_resam)
    # %%Plot!
    figure_configuration_ieee_standard()

    # %%
    k_scaling = 2
    fig_width = 8.8/2.54 * k_scaling
    fig_height = 4.4/2.54 * k_scaling
    params = {'figure.figsize': [fig_width, fig_height],
         'axes.labelsize': 8*k_scaling,  # fontsize for x and y labels (was 10)
         'axes.titlesize': 8*k_scaling,
         'font.size': 8*k_scaling,  # was 10
         'legend.fontsize': 8*k_scaling,  # was 10
         'xtick.labelsize': 8*k_scaling,
         'ytick.labelsize': 8*k_scaling,
         'xtick.major.width': 1*k_scaling,
         'xtick.major.size': 3.5*k_scaling,
         'ytick.major.width': 1*k_scaling,
         'ytick.major.size': 3.5*k_scaling,
         'lines.linewidth': 2.5*k_scaling,
         'axes.linewidth': 1*k_scaling,
         'axes.grid': True,
         'savefig.format': 'pdf',
         'axes.xmargin': 0,
         'axes.ymargin': 0,
         'savefig.pad_inches': 0,
         'legend.markerscale': 1*k_scaling,
         'savefig.bbox': 'tight',
         'lines.markersize': 3*k_scaling,
         #'legend.numpoints': 4*k_scaling,
         #'legend.handlelength': 3.5*k_scaling,
    }
    import matplotlib
    matplotlib.rcParams.update(params)
    plt.figure()
    plt.plot(t_fairness, np.cumsum(np.ones(N_points))-np.cumsum(sbuose_qoe_fairness), '-.', label='CBA-OS-SVI',color='tab:green')
    plt.plot(t_fairness, np.cumsum(np.ones(N_points))-np.cumsum(sbu_qoe_fairness), ':', label="CBA-VB",color='tab:green')
    plt.plot(t_fairness, np.cumsum(np.ones(N_points)) - np.cumsum(linucb_qoe_fairness), '^',  label="LinUCB",
             color='tab:orange', markevery=20)
    plt.plot(t_fairness, np.cumsum(np.ones(N_points))-np.cumsum(panda_qoe_fairness), '-', label="PANDA")
    plt.plot(t_fairness, np.cumsum(np.ones(N_points)) - np.cumsum(bola_qoe_fairness), '--', label="BOLA",color='tab:red')

    plt.xlabel("time [s]")
    #plt.xlim(0, 180)
    plt.ylabel("Regret of QoE Fairness")
    lgnd = plt.legend(loc='lower right')
    for lh in lgnd.legendHandles:
            lh._legmarker.set_markersize(7)
    fig = plt.gcf()
    fig.savefig('fairness{}'.format(suffix))
    plt.show()
    test=1

    #
    # plt.plot(mean_rew_f_oracle - mean_rew_f_sbo, '-.', label="OS-SVI")
    # plt.plot(mean_rew_f_oracle - mean_rew_f_sbsvi, '^', label="SVI", markevery=40)
    # plt.plot(mean_rew_f_oracle - mean_rew_f_sbvb, ':', label="VB")

    # # %%
    # plt.figure()
    # plt.step(t_sbuose, sbuose_quality, '-', label='CBA-OS-SVI')
    # plt.plot(t_sbu, sbu_quality, '--', label="CBA-VB")
    # plt.xlabel("time [s]")
    # plt.ylabel("Quality Level")
    # plt.legend()
    # fig = plt.gcf()
    # # fig.savefig('qoe_single')
    # plt.show()
    #
    # # %%
    # plt.figure()
    # plt.plot(t_sbuose, sbuose_qoe, '-', label='CBA-OS-SVI')
    # plt.plot(t_sbu, sbu_qoe, '--', label="CBA-VB")
    #
    # plt.xlabel("time [s]")
    # plt.ylabel("QoE")
    # plt.legend()
    # fig = plt.gcf()
    # # fig.savefig('qoe_single')
    # plt.show()
    #
    # # %%
    # plt.figure()
    # plt.plot(t_sbuose, sbuose_rebuf, '-', label='CBA-OS-SVI')
    # plt.plot(t_sbu, sbu_rebuf, '--', label="CBA-VB")
    # plt.xlabel("time [s]")
    # plt.ylabel("Rebuffering [ms]")
    # plt.legend()
    # fig = plt.gcf()
    # # fig.savefig('qoe_single')
    # plt.show()
    #
    # # %%
    # plt.figure()
    # plt.plot(t_sbuose, sbuose_extime, '-', label='CBA-OS-SVI')
    # plt.plot(t_sbu, sbu_extime, '--', label="CBA-VB")
    #
    # plt.xlabel("time [s]")
    # plt.ylabel("Runtime Learning [ms]")
    # plt.legend()
    # fig = plt.gcf()
    # # fig.savefig('qoe_single')
    # plt.show()
    #
    # # %%
    # plt.figure()
    # plt.step(np.cumsum(trace[0:30, 0]), trace[0:30, 1], '-')
    # plt.xlabel("time [s]")
    # plt.ylabel("Bandwidth [Mbit/s]")
    # fig = plt.gcf()
    # # fig.savefig('qoe_single')
    # plt.show()




def load_results(path):
    results = []

    with open(path, 'rb') as input:
        while 1:
            try:
                results.append(pickle.load(input))
            except EOFError:
                break
    input.close()
    return results


def fairness(qoe1, qoe2):
    relative_qoe = qoe1 / (qoe1+qoe2)
    mu_fairness = binary_entropy(relative_qoe)
    return mu_fairness


def binary_entropy(p):
    p = np.array(p, dtype=np.float32)
    h = -(1 - p) * np.log2(1 - p) - p * np.log2(p)
    return h


if __name__ == '__main__':
    main()
