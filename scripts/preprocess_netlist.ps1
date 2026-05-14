param(
    [Parameter(Mandatory=$true)]
    [string]$InputFile,

    [Parameter(Mandatory=$true)]
    [string]$OutputFile
)

# BRAM parameter names that need defparam conversion
$bramParams = @{
    "init_00"=$true; "init_01"=$true; "init_02"=$true; "init_03"=$true
    "init_04"=$true; "init_05"=$true; "init_06"=$true; "init_07"=$true
    "init_08"=$true; "init_09"=$true; "init_0a"=$true; "init_0b"=$true
    "init_0c"=$true; "init_0d"=$true; "init_0e"=$true; "init_0f"=$true
    "porta_attr"=$true; "portb_attr"=$true
    "clkamux"=$true; "enamux"=$true; "rstamux"=$true; "weamux"=$true
    "clkbmux"=$true; "enbmux"=$true; "rstbmux"=$true; "webmux"=$true
}

$defparamRegex = [regex]'defparam\s+(\S+)\.(\w+)\.CONF\s*=\s*"([^"]*)"\s*;'
$dupBaseRegex  = [regex]'(\d+''h)(\d+''h)'

$reader  = [System.IO.StreamReader]::new($InputFile, [System.Text.Encoding]::UTF8)
$writer  = [System.IO.StreamWriter]::new($OutputFile, $false, [System.Text.UTF8Encoding]::new($false))

try {
    while ($null -ne ($line = $reader.ReadLine())) {
        # 1) Fix Yosys duplicate base format: e.g. 276'h256'h... -> 256'h...
        $line = $dupBaseRegex.Replace($line, '$2')

        # 2) Convert defparam instance.param.CONF = "value";
        $m = $defparamRegex.Match($line)
        if ($m.Success) {
            $instancePath = $m.Groups[1].Value
            $paramName    = $m.Groups[2].Value
            $value        = $m.Groups[3].Value

            $isBramInstance = $instancePath -match 'Bram|bram'
            $isBramParam    = $bramParams.ContainsKey($paramName)

            if ($isBramInstance -and $isBramParam) {
                if ($paramName -like 'init_*') {
                    $line = "defparam $instancePath.$paramName = 256'h$value;"
                } else {
                    $line = "defparam $instancePath.$paramName = `"$value`";"
                }
            }
        }

        $writer.WriteLine($line)
    }
} finally {
    $reader.Close()
    $writer.Close()
}
