// The public repository needs no Jenkins credentials.
def checkoutLibivy(String revision, boolean trackChanges = false) {
    return checkout(changelog: trackChanges, poll: trackChanges, scm: [
        $class: 'GitSCM',
        branches: [[name: revision]],
        userRemoteConfigs: [[url: 'https://github.com/lii-enac/libivy.git']]
    ])
}

def selectedBuild(String name) {
    return !params.BUILD_ONLY?.trim() || params.BUILD_ONLY == 'all' || params.BUILD_ONLY == name
}

def notifyResult(String status) {
    def recipients = [env.EMAIL1, env.EMAIL2].findAll { it?.trim() }.join(', ')
    if (recipients) {
        emailext(
            to: recipients,
            subject: "libivy ${status} - ${env.JOB_NAME} #${env.BUILD_NUMBER}",
            mimeType: 'text/plain',
            body: """Build: ${env.BUILD_URL}
Commit: ${env.LIBIVY_REVISION ?: 'checkout failed'}

scan-build and clang-tidy reports (HTML, logs and ZIP):
${env.BUILD_URL}artifact/

Console: ${env.BUILD_URL}console
"""
        )
    }
}

pipeline {
    agent none

    parameters {
        choice(
            name: 'BUILD_ONLY',
            choices: ['', 'all', 'macos', 'ubuntu', 'windows', 'windows-wsl'],
            description: 'Empty/all builds every platform. macOS compilation and both macOS analyses always run; this selection limits the other platform builds.'
        )
    }

    options {
        buildDiscarder(logRotator(numToKeepStr: '10', artifactNumToKeepStr: '10'))
        disableConcurrentBuilds()
        skipDefaultCheckout(true)
        timestamps()
    }

    triggers {
        pollSCM('H/5 * * * *')
    }

    stages {
        stage('Build macOS') {
            agent { label 'macos12' }
            options { timeout(time: 15, unit: 'MINUTES') }
            environment {
                PATH = '/opt/homebrew/bin:/opt/homebrew/sbin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin'
            }
            steps {
                dir('libivy') {
                    deleteDir()
                    script {
                        def revision = checkoutLibivy('*/master', true)
                        env.LIBIVY_REVISION = revision.GIT_COMMIT
                        currentBuild.description = "libivy ${env.LIBIVY_REVISION.take(12)}"
                    }
                    // Retain the failure but still generate analysis reports for this commit.
                    catchError(buildResult: 'FAILURE', stageResult: 'FAILURE') {
                        sh '''
                            set -eu
                            make version
                            make -j8 static-libs shared-libs tools pkgconf \
                                CC="$(xcrun --find clang)" CPP="$(xcrun --find clang++)"
                            make check CC="$(xcrun --find clang)"
                        '''
                    }
                }
            }
        }

        stage('Analyses macOS and other platforms') {
            // No failFast: one platform/analysis failure must not cancel the others.
            parallel {
                stage('Static analysis macOS') {
                    agent { label 'macos12' }
                    options { timeout(time: 45, unit: 'MINUTES') }
                    environment {
                        // LLVM is selected explicitly; Apple compilers remain the build toolchain.
                        PATH = '/opt/homebrew/bin:/opt/homebrew/sbin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin'
                    }
                    stages {
                        stage('Checkout for analysis') {
                            steps {
                                dir('libivy-analysis') {
                                    deleteDir()
                                    script { checkoutLibivy(env.LIBIVY_REVISION) }
                                }
                            }
                        }
                        stage('scan-build') {
                            steps {
                                dir('libivy-analysis') {
                                    catchError(buildResult: 'FAILURE', stageResult: 'FAILURE') {
                                        sh 'python3 tools/analyze_macos.py scan-build --report-dir reports/scan-build --jobs 8'
                                    }
                                }
                            }
                        }
                        stage('clang-tidy') {
                            steps {
                                dir('libivy-analysis') {
                                    catchError(buildResult: 'FAILURE', stageResult: 'FAILURE') {
                                        sh 'python3 tools/analyze_macos.py tidy --report-dir reports/tidy --jobs 4'
                                    }
                                }
                            }
                        }
                    }
                    post {
                        always {
                            dir('libivy-analysis') {
                                sh '''
                                    if [ -d reports ]; then
                                        /usr/bin/zip -qr analysis-reports.zip reports
                                    fi
                                '''
                                archiveArtifacts artifacts: 'reports/**,analysis-reports.zip', allowEmptyArchive: true
                            }
                        }
                    }
                }

                stage('Build Ubuntu') {
                    when {
                        beforeAgent true
                        expression { selectedBuild('ubuntu') }
                    }
                    agent { label 'ubuntu' }
                    options { timeout(time: 15, unit: 'MINUTES') }
                    steps {
                        dir('libivy') {
                            deleteDir()
                            script { checkoutLibivy(env.LIBIVY_REVISION) }
                            sh '''
                                set -eu
                                make version
                                make -j8 static-libs shared-libs tools pkgconf
                                make check
                            '''
                        }
                    }
                }

                stage('Build Windows MSYS2') {
                    when {
                        beforeAgent true
                        expression { selectedBuild('windows') }
                    }
                    agent { label 'win10' }
                    tools { git 'Git-MSYS' }
                    options { timeout(time: 15, unit: 'MINUTES') }
                    steps {
                        dir('libivy') {
                            deleteDir()
                            script { checkoutLibivy(env.LIBIVY_REVISION) }
                            bat '''
                                @echo on
                                make version
                                if errorlevel 1 exit /b 1
                                make -j8 static-libs shared-libs tools pkgconf
                                if errorlevel 1 exit /b 1
                                make check
                                if errorlevel 1 exit /b 1
                            '''
                        }
                    }
                }

                stage('Build Windows WSL') {
                    when {
                        beforeAgent true
                        expression { selectedBuild('windows-wsl') }
                    }
                    agent { label 'wsl' }
                    options { timeout(time: 15, unit: 'MINUTES') }
                    steps {
                        dir('libivy') {
                            deleteDir()
                            script { checkoutLibivy(env.LIBIVY_REVISION) }
                            sh '''
                                set -eu
                                make version
                                make -j8 static-libs shared-libs tools pkgconf
                                make check
                            '''
                        }
                    }
                }
            }
        }
    }

    post {
        success { script { notifyResult('SUCCESS') } }
        failure { script { notifyResult('FAILURE') } }
        unstable { script { notifyResult('UNSTABLE') } }
    }
}
